/*
 * control.c
 * MACT library controller handling
 * 
 * Derived from MACT386.LIB disassembly by Jonathon Fowler
 *
 */

#include "types.h"
#include "keyboard.h"
#include "mouse.h"
#include "control.h"
#include "_control.h"
#include "_functio.h"
#include "util_lib.h"

#include "baselayer.h"
#include "compat.h"
#include "pragmas.h"

#include "dnAPI.h"

#if !defined(max)
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#if !defined(min)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif


boolean CONTROL_JoyPresent = false;
boolean CONTROL_JoystickEnabled = false;
boolean CONTROL_MousePresent = false;
boolean CONTROL_MouseEnabled = false;
uint32  CONTROL_ButtonState1 = 0;
uint32  CONTROL_ButtonHeldState1 = 0;
uint32  CONTROL_ButtonState2 = 0;
uint32  CONTROL_ButtonHeldState2 = 0;

static int32 CONTROL_UserInputDelay = -1;
static int32 CONTROL_MouseSensitivity = DEFAULTMOUSESENSITIVITY;
static int32 CONTROL_NumMouseButtons = 0;
static int32 CONTROL_NumMouseAxes = 0;
static int32 CONTROL_NumJoyButtons = 0;
static int32 CONTROL_NumJoyAxes = 0;
static controlflags       CONTROL_Flags[CONTROL_NUM_FLAGS];
static controlbuttontype  CONTROL_MouseButtonMapping[MAXMOUSEBUTTONS],
                          CONTROL_JoyButtonMapping[MAXJOYBUTTONS];
static controlkeymaptype  CONTROL_KeyMapping[CONTROL_NUM_FLAGS];
static controlaxismaptype CONTROL_MouseAxesMap[MAXMOUSEAXES],	// maps physical axes onto virtual ones
                          CONTROL_JoyAxesMap[MAXJOYAXES];
static controlaxistype    CONTROL_MouseAxes[MAXMOUSEAXES],	// physical axes
                          CONTROL_JoyAxes[MAXJOYAXES];
static controlaxistype    CONTROL_LastMouseAxes[MAXMOUSEAXES],
                          CONTROL_LastJoyAxes[MAXJOYAXES];
static int32   CONTROL_MouseAxesScale[MAXMOUSEAXES],             CONTROL_JoyAxesScale[MAXJOYAXES];
static int32   CONTROL_MouseButtonState[MAXMOUSEBUTTONS],        CONTROL_JoyButtonState[MAXJOYBUTTONS];
static int32   CONTROL_MouseButtonClickedTime[MAXMOUSEBUTTONS],  CONTROL_JoyButtonClickedTime[MAXJOYBUTTONS];
static boolean CONTROL_MouseButtonClickedState[MAXMOUSEBUTTONS], CONTROL_JoyButtonClickedState[MAXJOYBUTTONS];
static boolean CONTROL_MouseButtonClicked[MAXMOUSEBUTTONS],      CONTROL_JoyButtonClicked[MAXJOYBUTTONS];
static byte    CONTROL_MouseButtonClickedCount[MAXMOUSEBUTTONS], CONTROL_JoyButtonClickedCount[MAXJOYBUTTONS];
static boolean CONTROL_UserInputCleared[3];
static int32 (*CtlGetTime)(void);
static boolean CONTROL_Started = false;
static int32 ticrate;
static int32 CONTROL_DoubleClickSpeed;


void CONTROL_GetMouseDelta(void)
{
	int32 x,y;

	MOUSE_GetDelta(&x, &y);

	/* What in the name of all things sacred is this?
	if (labs(*x) > labs(*y)) {
		*x /= 3;
	} else {
		*y /= 3;
	}
	*y = *y * 96;
	*x = (*x * 32 * CONTROL_MouseSensitivity) >> 15;
	*/

	//CONTROL_MouseAxes[0].analog = mulscale8(x, CONTROL_MouseSensitivity);
	//CONTROL_MouseAxes[1].analog = mulscale6(y, CONTROL_MouseSensitivity);
	
	// DOS axis coefficients = 32,96
	CONTROL_MouseAxes[0].analog = (x * 48 * CONTROL_MouseSensitivity) >> 15;
	CONTROL_MouseAxes[1].analog = (y * 48*4 * CONTROL_MouseSensitivity) >> 15;
}

int32 CONTROL_GetMouseSensitivity(void)
{
	return (CONTROL_MouseSensitivity - MINIMUMMOUSESENSITIVITY);
}

void CONTROL_SetMouseSensitivity(int32 newsensitivity)
{
	CONTROL_MouseSensitivity = newsensitivity + MINIMUMMOUSESENSITIVITY;
}

boolean CONTROL_StartMouse(void)
{
	CONTROL_NumMouseButtons = MAXMOUSEBUTTONS;
	return MOUSE_Init();
}

void CONTROL_GetJoyAbs( void )
{
}

void CONTROL_FilterJoyDelta(void)
{
}

void CONTROL_GetJoyDelta( void )
{
	int32 i;
	
	for (i=0; i<joynumaxes; i++)
		CONTROL_JoyAxes[i].analog = joyaxis[i] >> 5;
}


void CONTROL_SetJoyScale( void )
{
}

void CONTROL_CenterJoystick
   (
   void ( *CenterCenter )( void ),
   void ( *UpperLeft )( void ),
   void ( *LowerRight )( void ),
   void ( *CenterThrottle )( void ),
   void ( *CenterRudder )( void )
   )
{
}

boolean CONTROL_StartJoy(int32 joy)
{
	return (inputdevices & 4) == 4;
}

void CONTROL_ShutJoy(int32 joy)
{
	CONTROL_JoyPresent = false;
}

int32 CONTROL_GetTime(void)
{
	static int32 t = 0;
	t += 5;
	return t;
}

boolean CONTROL_CheckRange(int32 which)
{
	if ((uint32)which < (uint32)CONTROL_NUM_FLAGS) return false;
	//Error("CONTROL_CheckRange: Index %d out of valid range for %d control flags.",
	//	which, CONTROL_NUM_FLAGS);
	return true;
}

void CONTROL_SetFlag(int32 which, boolean active)
{
	if (CONTROL_CheckRange(which)) return;

	if (CONTROL_Flags[which].toggle == INSTANT_ONOFF) {
		CONTROL_Flags[which].active = active?1:0;
	} else {
		if (active) {
			CONTROL_Flags[which].buttonheld = false;
		} else if (CONTROL_Flags[which].buttonheld == false) {
			CONTROL_Flags[which].buttonheld = true;
			CONTROL_Flags[which].active = (CONTROL_Flags[which].active ? false : true);
		}
	}
}

boolean CONTROL_KeyboardFunctionPressed(int32 which)
{
	boolean key1 = 0, key2 = 0;

	if (CONTROL_CheckRange(which)) return false;

	if (!CONTROL_Flags[which].used) return false;

	if (CONTROL_KeyMapping[which].key1 != KEYUNDEFINED && CONTROL_KeyMapping[which].key1 != KEYUNDEFINED2)
		key1 = KB_KeyDown[ CONTROL_KeyMapping[which].key1 ] ? true : false;

	if (CONTROL_KeyMapping[which].key2 != KEYUNDEFINED && CONTROL_KeyMapping[which].key2 != KEYUNDEFINED2)
		key2 = KB_KeyDown[ CONTROL_KeyMapping[which].key2 ] ? true : false;

	return (key1 | key2);
}

void CONTROL_ClearKeyboardFunction(int32 which)
{
	if (CONTROL_CheckRange(which)) return;

	if (!CONTROL_Flags[which].used) return;

	if (CONTROL_KeyMapping[which].key1 != KEYUNDEFINED)
		KB_KeyDown[ CONTROL_KeyMapping[which].key1 ] = 0;

	if (CONTROL_KeyMapping[which].key2 != KEYUNDEFINED)
		KB_KeyDown[ CONTROL_KeyMapping[which].key2 ] = 0;
}

void CONTROL_DefineFlag( int32 which, boolean toggle )
{
	if (CONTROL_CheckRange(which)) return;

	CONTROL_Flags[which].active     = false;
	CONTROL_Flags[which].used       = true;
	CONTROL_Flags[which].toggle     = toggle?1:0;
	CONTROL_Flags[which].buttonheld = false;
	CONTROL_Flags[which].cleared    = 0;
}

boolean CONTROL_FlagActive( int32 which )
{
	if (CONTROL_CheckRange(which)) return false;

	return CONTROL_Flags[which].used;
}

static
int CONTROL_TranslateButton(int button) {
    switch (button) {
        case 1: return 2;
        case 2: return 1;
    }
    return button;
}

void CONTROL_MapKey( int32 which, kb_scancode key1, kb_scancode key2 )
{
	if (CONTROL_CheckRange(which)) return;

	CONTROL_KeyMapping[which].key1 = key1 ? key1 : KEYUNDEFINED;
	CONTROL_KeyMapping[which].key2 = key2 ? key2 : KEYUNDEFINED;

    if (key1 > 0 && key1 & 0x10000) { /* mouse button */
        int button = CONTROL_TranslateButton((key1 & 0xFFFF)-1);
        CONTROL_MapButton(which, button, 0, controldevice_mouse);
    }
    if (key2 > 0 && key2 & 0x10000) {
        int button = CONTROL_TranslateButton((key2 & 0xFFFF)-1);
        CONTROL_MapButton(which, button, 0, controldevice_mouse);
    }
}

void CONTROL_PrintKeyMap(void)
{
	int32 i;

	for (i=0;i<CONTROL_NUM_FLAGS;i++) {
		initprintf("function %2ld key1=%3x key2=%3x\n",
			i, CONTROL_KeyMapping[i].key1, CONTROL_KeyMapping[i].key2);
	}
}

void CONTROL_PrintControlFlag(int32 which)
{
	initprintf("function %2ld active=%d used=%d toggle=%d buttonheld=%d cleared=%ld\n",
		which, CONTROL_Flags[which].active, CONTROL_Flags[which].used,
		CONTROL_Flags[which].toggle, CONTROL_Flags[which].buttonheld,
		CONTROL_Flags[which].cleared);
}

void CONTROL_PrintAxes(void)
{
	int32 i;

	initprintf("nummouseaxes=%ld\n", CONTROL_NumMouseAxes);
	for (i=0;i<CONTROL_NumMouseAxes;i++) {
		initprintf("axis=%ld analog=%d digital1=%d digital2=%d\n",
				i, CONTROL_MouseAxesMap[i].analogmap,
				CONTROL_MouseAxesMap[i].minmap, CONTROL_MouseAxesMap[i].maxmap);
	}

	initprintf("numjoyaxes=%ld\n", CONTROL_NumJoyAxes);
	for (i=0;i<CONTROL_NumJoyAxes;i++) {
		initprintf("axis=%ld analog=%d digital1=%d digital2=%d\n",
				i, CONTROL_JoyAxesMap[i].analogmap,
				CONTROL_JoyAxesMap[i].minmap, CONTROL_JoyAxesMap[i].maxmap);
	}
}

void CONTROL_MapButton( int32 whichfunction, int32 whichbutton, boolean doubleclicked, controldevice device )
{
	controlbuttontype *set;
	
	if (CONTROL_CheckRange(whichfunction)) whichfunction = BUTTONUNDEFINED;

	switch (device) {
		case controldevice_mouse:
			if ((uint32)whichbutton >= (uint32)MAXMOUSEBUTTONS) {
				//Error("CONTROL_MapButton: button %d out of valid range for %d mouse buttons.",
				//		whichbutton, CONTROL_NumMouseButtons);
				return;
			}
			set = CONTROL_MouseButtonMapping;
			break;

		case controldevice_joystick:
			if ((uint32)whichbutton >= (uint32)MAXJOYBUTTONS) {
				//Error("CONTROL_MapButton: button %d out of valid range for %d joystick buttons.",
				//		whichbutton, CONTROL_NumJoyButtons);
				return;
			}
			set = CONTROL_JoyButtonMapping;
			break;

		default:
			//Error("CONTROL_MapButton: invalid controller device type");
			return;
	}
	
	if (doubleclicked)
		set[whichbutton].doubleclicked = (byte)whichfunction;
	else
		set[whichbutton].singleclicked = (byte)whichfunction;
}

void CONTROL_MapAnalogAxis( int32 whichaxis, int32 whichanalog, controldevice device )
{
	controlaxismaptype *set;

	if ((uint32)whichanalog >= (uint32)analog_maxtype) {
		//Error("CONTROL_MapAnalogAxis: analog function %d out of valid range for %d analog functions.",
		//		whichanalog, analog_maxtype);
		return;
	}

	switch (device) {
		case controldevice_mouse:
			if ((uint32)whichaxis >= (uint32)MAXMOUSEAXES) {
				//Error("CONTROL_MapAnalogAxis: axis %d out of valid range for %d mouse axes.",
				//		whichaxis, MAXMOUSEAXES);
				return;
			}

			set = CONTROL_MouseAxesMap;
			break;

		case controldevice_joystick:
			if ((uint32)whichaxis >= (uint32)MAXJOYAXES) {
				//Error("CONTROL_MapAnalogAxis: axis %d out of valid range for %d joystick axes.",
				//		whichaxis, MAXJOYAXES);
				return;
			}

			set = CONTROL_JoyAxesMap;
			break;

		default:
			//Error("CONTROL_MapAnalogAxis: invalid controller device type");
			return;
	}

	set[whichaxis].analogmap = (byte)whichanalog;
}

void CONTROL_SetAnalogAxisScale( int32 whichaxis, int32 axisscale, controldevice device )
{
	int32 *set;

	switch (device) {
		case controldevice_mouse:
			if ((uint32)whichaxis >= (uint32)MAXMOUSEAXES) {
				//Error("CONTROL_SetAnalogAxisScale: axis %d out of valid range for %d mouse axes.",
				//		whichaxis, MAXMOUSEAXES);
				return;
			}

			set = CONTROL_MouseAxesScale;
			break;

		case controldevice_joystick:
			if ((uint32)whichaxis >= (uint32)MAXJOYAXES) {
				//Error("CONTROL_SetAnalogAxisScale: axis %d out of valid range for %d joystick axes.",
				//		whichaxis, MAXJOYAXES);
				return;
			}

			set = CONTROL_JoyAxesScale;
			break;

		default:
			//Error("CONTROL_SetAnalogAxisScale: invalid controller device type");
			return;
	}
	
	set[whichaxis] = axisscale;
}

void CONTROL_MapDigitalAxis( int32 whichaxis, int32 whichfunction, int32 direction, controldevice device )
{
	controlaxismaptype *set;

	if (CONTROL_CheckRange(whichfunction)) whichfunction = AXISUNDEFINED;

	switch (device) {
		case controldevice_mouse:
			if ((uint32)whichaxis >= (uint32)MAXMOUSEAXES) {
				//Error("CONTROL_MapDigitalAxis: axis %d out of valid range for %d mouse axes.",
				//		whichaxis, MAXMOUSEAXES);
				return;
			}

			set = CONTROL_MouseAxesMap;
			break;

		case controldevice_joystick:
			if ((uint32)whichaxis >= (uint32)MAXJOYAXES) {
				//Error("CONTROL_MapDigitalAxis: axis %d out of valid range for %d joystick axes.",
				//		whichaxis, MAXJOYAXES);
				return;
			}

			set = CONTROL_JoyAxesMap;
			break;

		default:
			//Error("CONTROL_MapDigitalAxis: invalid controller device type");
			return;
	}
	
	switch (direction) {	// JBF: this is all very much a guess. The ASM puzzles me.
		case axis_up:
		case axis_left:
			set[whichaxis].minmap = (byte)whichfunction;
			break;
		case axis_down:
		case axis_right:
			set[whichaxis].maxmap = (byte)whichfunction;
			break;
		default:
			break;
	}
}

void CONTROL_ClearFlags(void)
{
	int32 i;

	for (i=0;i<CONTROL_NUM_FLAGS;i++)
		CONTROL_Flags[i].used = false;
}

void CONTROL_ClearAssignments( void )
{
	int32 i;

	memset(CONTROL_MouseButtonMapping,  BUTTONUNDEFINED, sizeof(CONTROL_MouseButtonMapping));
	memset(CONTROL_JoyButtonMapping,    BUTTONUNDEFINED, sizeof(CONTROL_JoyButtonMapping));
	memset(CONTROL_KeyMapping,          KEYUNDEFINED,    sizeof(CONTROL_KeyMapping));
	memset(CONTROL_MouseAxesMap,        AXISUNDEFINED,   sizeof(CONTROL_MouseAxesMap));
	memset(CONTROL_JoyAxesMap,          AXISUNDEFINED,   sizeof(CONTROL_JoyAxesMap));
	memset(CONTROL_MouseAxes,           0,               sizeof(CONTROL_MouseAxes));
	memset(CONTROL_JoyAxes,             0,               sizeof(CONTROL_JoyAxes));
	memset(CONTROL_LastMouseAxes,       0,               sizeof(CONTROL_LastMouseAxes));
	memset(CONTROL_LastJoyAxes,         0,               sizeof(CONTROL_LastJoyAxes));
	for (i=0;i<MAXMOUSEAXES;i++)
		CONTROL_MouseAxesScale[i] = NORMALAXISSCALE;
	for (i=0;i<MAXJOYAXES;i++)
		CONTROL_JoyAxesScale[i] = NORMALAXISSCALE;
}

static void DoGetDeviceButtons(
	int32 buttons, int32 tm,
	int32 NumButtons,
	int32 *DeviceButtonState,
	int32 *ButtonClickedTime,
	boolean *ButtonClickedState,
	boolean *ButtonClicked,
	byte  *ButtonClickedCount
) {
	int32 i, bs;

	for (i=0;i<NumButtons;i++) {
		bs = (buttons >> i) & 1;

		DeviceButtonState[i] = bs;
		ButtonClickedState[i] = false;

		if (bs) {
			if (ButtonClicked[i] == false) {
				ButtonClicked[i] = true;

				if (ButtonClickedCount[i] == 0 || tm > ButtonClickedTime[i]) {
					ButtonClickedTime[i] = tm + CONTROL_DoubleClickSpeed;
					ButtonClickedCount[i] = 1;
				}
				else if (tm < ButtonClickedTime[i]) {
					ButtonClickedState[i] = true;
					ButtonClickedTime[i]  = 0;
					ButtonClickedCount[i] = 2;
				}
			}
			else if (ButtonClickedCount[i] == 2) {
				ButtonClickedState[i] = true;
			}
		} else {
			if (ButtonClickedCount[i] == 2)
				ButtonClickedCount[i] = 0;

			ButtonClicked[i] = false;
		}
	}
}

void CONTROL_GetDeviceButtons(void)
{
	int32 t;
    
	t = CtlGetTime();

	if (CONTROL_MouseEnabled) {
		DoGetDeviceButtons(
			MOUSE_GetButtons(), t,
			CONTROL_NumMouseButtons,
			CONTROL_MouseButtonState,
			CONTROL_MouseButtonClickedTime,
			CONTROL_MouseButtonClickedState,
			CONTROL_MouseButtonClicked,
			CONTROL_MouseButtonClickedCount
		);
	}

	if (CONTROL_JoystickEnabled) {
		int32 buttons = joyb;
		if (joynumhats > 0 && joyhat[0] != -1) {
			static int32 hatstate[] = { 1, 1|2, 2, 2|4, 4, 4|8, 8, 8|1 };
			int val;

			// thanks SDL for this much more sensible method
			val = ((joyhat[0] + 4500 / 2) % 36000) / 4500;
			if (val < 8) buttons |= hatstate[val] << min(MAXJOYBUTTONS,joynumbuttons);
		}

		DoGetDeviceButtons(
			buttons, t,
			CONTROL_NumJoyButtons,
			CONTROL_JoyButtonState,
			CONTROL_JoyButtonClickedTime,
			CONTROL_JoyButtonClickedState,
			CONTROL_JoyButtonClicked,
			CONTROL_JoyButtonClickedCount
		);
	}
}

void CONTROL_DigitizeAxis(int32 axis, controldevice device)
{
	controlaxistype *set, *lastset;
	
	switch (device) {
		case controldevice_mouse:
			set = CONTROL_MouseAxes;
			lastset = CONTROL_LastMouseAxes;
			break;

		case controldevice_joystick:
			set = CONTROL_JoyAxes;
			lastset = CONTROL_LastJoyAxes;
			break;

		default: return;
	}
	
	if (set[axis].analog > 0) {
		if (set[axis].analog > THRESHOLD) {			// if very much in one direction,
			set[axis].digital = 1;				// set affirmative
		} else {
			if (set[axis].analog > MINTHRESHOLD) {		// if hanging in limbo,
				if (lastset[axis].digital == 1)	// set if in same direction as last time
					set[axis].digital = 1;
			}
		}
	} else {
		if (set[axis].analog < -THRESHOLD) {
			set[axis].digital = -1;
		} else {
			if (set[axis].analog < -MINTHRESHOLD) {
				if (lastset[axis].digital == -1)
					set[axis].digital = -1;
			}
		}
	}
}

void CONTROL_ScaleAxis(int32 axis, controldevice device)
{
	controlaxistype *set;
	int32 *scale;
	
	switch (device) {
		case controldevice_mouse:
			set = CONTROL_MouseAxes;
			scale = CONTROL_MouseAxesScale;
			break;

		case controldevice_joystick:
			set = CONTROL_JoyAxes;
			scale = CONTROL_JoyAxesScale;
			break;

		default: return;
	}

	set[axis].analog = mulscale16(set[axis].analog, scale[axis]);
}

void CONTROL_ApplyAxis(int32 axis, ControlInfo *info, controldevice device)
{
	controlaxistype *set;
	controlaxismaptype *map;
	
	switch (device) {
		case controldevice_mouse:
			set = CONTROL_MouseAxes;
			map = CONTROL_MouseAxesMap;
			break;

		case controldevice_joystick:
			set = CONTROL_JoyAxes;
			map = CONTROL_JoyAxesMap;
			break;

		default: return;
	}

	switch (map[axis].analogmap) {
		case analog_turning:          info->dyaw   += set[axis].analog; break;
		case analog_strafing:         info->dx     += set[axis].analog; break;
		case analog_lookingupanddown: info->dpitch += set[axis].analog; break;
		case analog_elevation:        info->dy     += set[axis].analog; break;
		case analog_rolling:          info->droll  += set[axis].analog; break;
		case analog_moving:           info->dz     += set[axis].analog; break;
		default: break;
	}
}

void CONTROL_PollDevices(ControlInfo *info)
{
	int32 i;

	memcpy(CONTROL_LastMouseAxes, CONTROL_MouseAxes, sizeof(CONTROL_MouseAxes));
	memcpy(CONTROL_LastJoyAxes,   CONTROL_JoyAxes,   sizeof(CONTROL_JoyAxes));

	memset(CONTROL_MouseAxes, 0, sizeof(CONTROL_MouseAxes));
	memset(CONTROL_JoyAxes,   0, sizeof(CONTROL_JoyAxes));
	memset(info, 0, sizeof(ControlInfo));

#if 0
	if (CONTROL_MouseEnabled) {
		CONTROL_GetMouseDelta();

		for (i=0; i<MAXMOUSEAXES; i++) {
			CONTROL_DigitizeAxis(i, controldevice_mouse);
			CONTROL_ScaleAxis(i, controldevice_mouse);
			LIMITCONTROL(&CONTROL_MouseAxes[i].analog);
			CONTROL_ApplyAxis(i, info, controldevice_mouse);
		}
	}
#endif
	if (CONTROL_JoystickEnabled) {
		CONTROL_GetJoyDelta();

		// Why?
		//CONTROL_Axes[0].analog /= 2;
		//CONTROL_Axes[2].analog /= 2;

		for (i=0; i<MAXJOYAXES; i++) {
			CONTROL_DigitizeAxis(i, controldevice_joystick);
			CONTROL_ScaleAxis(i, controldevice_joystick);
			LIMITCONTROL(&CONTROL_JoyAxes[i].analog);
			CONTROL_ApplyAxis(i, info, controldevice_joystick);
		}
	}
	
	CONTROL_GetDeviceButtons();
}

void CONTROL_AxisFunctionState(int32 *p1)
{
	int32 i, j;

    #if 0
	for (i=0; i<CONTROL_NumMouseAxes; i++) {
		if (!CONTROL_MouseAxes[i].digital) continue;

		if (CONTROL_MouseAxes[i].digital < 0)
			j = CONTROL_MouseAxesMap[i].minmap;
		else
			j = CONTROL_MouseAxesMap[i].maxmap;

		if (j != AXISUNDEFINED)
			p1[j] = 1;
	}
	#endif

	for (i=0; i<CONTROL_NumJoyAxes; i++) {
		if (!CONTROL_JoyAxes[i].digital) continue;

		if (CONTROL_JoyAxes[i].digital < 0)
			j = CONTROL_JoyAxesMap[i].minmap;
		else
			j = CONTROL_JoyAxesMap[i].maxmap;

		if (j != AXISUNDEFINED)
			p1[j] = 1;
	}
}

void CONTROL_ButtonFunctionState( int32 *p1 )
{
	int32 i, j;

	for (i=0; i<CONTROL_NumMouseButtons; i++) {
		j = CONTROL_MouseButtonMapping[i].doubleclicked;
		if (j != KEYUNDEFINED && j != KEYUNDEFINED2 && j != 127)
			p1[j] |= CONTROL_MouseButtonClickedState[i];

		j = CONTROL_MouseButtonMapping[i].singleclicked;
		if (j != KEYUNDEFINED && j != KEYUNDEFINED2 && j != 127) {
			p1[j] |= CONTROL_MouseButtonState[i];
            if (p1[j]) {
                printf("%s activated\n", gamefunctions[j]);
            }
        }
	}

	for (i=0; i<CONTROL_NumJoyButtons; i++) {
		j = CONTROL_JoyButtonMapping[i].doubleclicked;
		if (j != KEYUNDEFINED)
			p1[j] |= CONTROL_JoyButtonClickedState[i];

		j = CONTROL_JoyButtonMapping[i].singleclicked;
		if (j != KEYUNDEFINED)
			p1[j] |= CONTROL_JoyButtonState[i];
	}
}

void CONTROL_GetUserInput( UserInput *info )
{
	ControlInfo ci;

	CONTROL_PollDevices( &ci );

	info->dir = dir_None;

	// checks if CONTROL_UserInputDelay is too far in the future due to clock skew?
	if (CtlGetTime() + ((ticrate * USERINPUTDELAY) / 1000) < CONTROL_UserInputDelay)
		CONTROL_UserInputDelay = -1;

	if (CtlGetTime() >= CONTROL_UserInputDelay) {
		if (CONTROL_MouseAxes[1].digital == -1)
			info->dir = dir_North;
		else if (CONTROL_MouseAxes[1].digital == 1)
			info->dir = dir_South;
		else if (CONTROL_MouseAxes[0].digital == -1)
			info->dir = dir_West;
		else if (CONTROL_MouseAxes[0].digital == 1)
			info->dir = dir_East;

		if (CONTROL_JoyAxes[1].digital == -1)
			info->dir = dir_North;
		else if (CONTROL_JoyAxes[1].digital == 1)
			info->dir = dir_South;
		else if (CONTROL_JoyAxes[0].digital == -1)
			info->dir = dir_West;
		else if (CONTROL_JoyAxes[0].digital == 1)
			info->dir = dir_East;
	}

	info->button0 = CONTROL_MouseButtonState[0] | CONTROL_JoyButtonState[0];
	info->button1 = CONTROL_MouseButtonState[1] | CONTROL_JoyButtonState[1];

	if (KB_KeyDown[sc_kpad_8] || KB_KeyDown[sc_UpArrow])
		info->dir = dir_North;
	else if (KB_KeyDown[sc_kpad_2] || KB_KeyDown[sc_DownArrow])
		info->dir = dir_South;
	else if (KB_KeyDown[sc_kpad_4] || KB_KeyDown[sc_LeftArrow])
		info->dir = dir_West;
	else if (KB_KeyDown[sc_kpad_6] || KB_KeyDown[sc_RightArrow])
		info->dir = dir_East;

	if (KB_KeyDown[BUTTON0_SCAN_1] || KB_KeyDown[BUTTON0_SCAN_2] || KB_KeyDown[BUTTON0_SCAN_3])
		info->button0 = 1;
	if (KB_KeyDown[BUTTON1_SCAN])
		info->button1 = 1;

	if (CONTROL_UserInputCleared[1]) {
		if (!info->button0)
			CONTROL_UserInputCleared[1] = false;
		else
			info->button0 = false;
	}
	if (CONTROL_UserInputCleared[2]) {
		if (!info->button1)
			CONTROL_UserInputCleared[2] = false;
		else
			info->button1 = false;
	}
}

void CONTROL_ClearUserInput( UserInput *info )
{
	switch (info->dir) {
		case dir_North:
		case dir_South:
		case dir_East:
		case dir_West:
			CONTROL_UserInputCleared[0] = true;
			CONTROL_UserInputDelay = CtlGetTime() + ((ticrate * USERINPUTDELAY) / 1000);
			switch (info->dir) {
				case dir_North: KB_KeyDown[sc_UpArrow]    = KB_KeyDown[sc_kpad_8] = 0; break;
				case dir_South: KB_KeyDown[sc_DownArrow]  = KB_KeyDown[sc_kpad_2] = 0; break;
				case dir_East:  KB_KeyDown[sc_LeftArrow]  = KB_KeyDown[sc_kpad_4] = 0; break;
				case dir_West:  KB_KeyDown[sc_RightArrow] = KB_KeyDown[sc_kpad_6] = 0; break;
				default: break;
			}
			break;
		default: break;
	}
	if (info->button0) CONTROL_UserInputCleared[1] = true;
	if (info->button1) CONTROL_UserInputCleared[2] = true;
}

void CONTROL_ClearButton( int32 whichbutton )
{
	if (CONTROL_CheckRange( whichbutton )) return;
	BUTTONCLEAR( whichbutton );
	CONTROL_Flags[whichbutton].cleared = true;
}

void CONTROL_GetInput( ControlInfo *info )
{
	int32 i, periphs[CONTROL_NUM_FLAGS];

	CONTROL_PollDevices( info );

	memset(periphs, 0, sizeof(periphs));
	CONTROL_ButtonFunctionState(periphs);
	CONTROL_AxisFunctionState(periphs);

	CONTROL_ButtonHeldState1 = CONTROL_ButtonState1;
	CONTROL_ButtonHeldState2 = CONTROL_ButtonState2;
	CONTROL_ButtonState1 = CONTROL_ButtonState2 = 0;

	for (i=0; i<CONTROL_NUM_FLAGS; i++) {
		CONTROL_SetFlag(i, CONTROL_KeyboardFunctionPressed(i) | periphs[i]);

		if (CONTROL_Flags[i].cleared == false) {
			BUTTONSET(i, CONTROL_Flags[i].active);
		}
		else if (CONTROL_Flags[i].active == false) {
			CONTROL_Flags[i].cleared = 0;
		}
	}
    dnGetInput();
}

void CONTROL_WaitRelease( void )
{
}

void CONTROL_Ack( void )
{
}

boolean CONTROL_Startup(controltype which, int32 ( *TimeFunction )( void ), int32 ticspersecond)
{
	int32 i;
	
	if (CONTROL_Started) return false;
    
	if (TimeFunction) CtlGetTime = TimeFunction;
	else CtlGetTime = CONTROL_GetTime;

	ticrate = ticspersecond;

	CONTROL_DoubleClickSpeed = (ticspersecond*57)/100;
	if (CONTROL_DoubleClickSpeed <= 0)
		CONTROL_DoubleClickSpeed = 1;

	if (initinput()) return true;

	CONTROL_MousePresent = CONTROL_MouseEnabled    = false;
	CONTROL_JoyPresent   = CONTROL_JoystickEnabled = false;
	CONTROL_NumMouseButtons = CONTROL_NumJoyButtons = 0;
	CONTROL_NumMouseAxes    = CONTROL_NumJoyAxes    = 0;
	KB_Startup();

	//switch (which) {
	//	case controltype_keyboard:
	//		break;

	//	case controltype_keyboardandmouse:
			CONTROL_NumMouseAxes      = MAXMOUSEAXES;
			CONTROL_NumMouseButtons   = MAXMOUSEBUTTONS;
			CONTROL_MousePresent      = MOUSE_Init();
			CONTROL_MouseEnabled      = CONTROL_MousePresent;
	//		break;

	//	case controltype_keyboardandjoystick:
			CONTROL_NumJoyAxes    = min(MAXJOYAXES,joynumaxes);
			CONTROL_NumJoyButtons = min(MAXJOYBUTTONS,joynumbuttons + 4*(joynumhats>0));
			CONTROL_JoyPresent    = CONTROL_StartJoy(0);
			CONTROL_JoystickEnabled = CONTROL_JoyPresent;
	//		break;
	//}
	
	if (CONTROL_MousePresent)
		initprintf("CONTROL_Startup: Mouse Present\n");
	if (CONTROL_JoyPresent)
		initprintf("CONTROL_Startup: Joystick Present\n");

	CONTROL_ButtonState1     = 0;
	CONTROL_ButtonState2     = 0;
	CONTROL_ButtonHeldState1 = 0;
	CONTROL_ButtonHeldState2 = 0;

	memset(CONTROL_UserInputCleared, 0, sizeof(CONTROL_UserInputCleared));

	for (i=0; i<CONTROL_NUM_FLAGS; i++)
		CONTROL_Flags[i].used = false;

    //dnResetBindings();
    dnApplyBindings();
    
	CONTROL_Started = true;

	return false;
}

void CONTROL_Shutdown(void)
{
	if (!CONTROL_Started) return;

	CONTROL_JoyPresent = false;

	MOUSE_Shutdown();
	uninitinput();

	CONTROL_Started = false;
}

/* new control system */

static uint32_t DN_ButtonState1 = 0;
static uint32_t DN_ButtonState2 = 0;
static uint32_t DN_AutoRelease1 = 0;
static uint32_t DN_AutoRelease2 = 0;

#define DN_BUTTONSET(x,value) \
(\
((x)>31) ?\
(DN_ButtonState2 |= (value<<((x)-32)))  :\
(DN_ButtonState1 |= (value<<(x)))\
)

#define DN_BUTTONCLEAR(x) \
(\
((x)>31) ?\
(DN_ButtonState2 &= (~(1<<((x)-32)))) :\
(DN_ButtonState1 &= (~(1<<(x))))\
)

#define DN_AUTORELEASESET(x,value) \
(\
((x)>31) ?\
(DN_AutoRelease2 |= (value<<((x)-32)))  :\
(DN_AutoRelease1 |= (value<<(x)))\
)


static int dnKeyMapping[DN_MAX_KEYS] = { -1 };
static dnKey dnFuncBindings[64][2] = { 0 };
static char *dnKeyNames[DN_MAX_KEYS] = { 0 };

void dnInitKeyNames() {
    dnKeyNames[SDLK_UNKNOWN] = "None";
    dnKeyNames[SDLK_BACKSPACE] = "Backspace";
    dnKeyNames[SDLK_TAB] = "Tab";
    dnKeyNames[SDLK_RETURN] = "Enter";
    dnKeyNames[SDLK_ESCAPE] = "Escape";
    dnKeyNames[SDLK_SPACE] = "Space";
    dnKeyNames[SDLK_EXCLAIM] = "!";
    dnKeyNames[SDLK_QUOTEDBL] = "\"";
    dnKeyNames[SDLK_HASH] = "#";
    dnKeyNames[SDLK_DOLLAR] = "$";
    dnKeyNames[SDLK_AMPERSAND] = "&";
    dnKeyNames[SDLK_QUOTE] = "'";
    dnKeyNames[SDLK_LEFTPAREN] = "(";
    dnKeyNames[SDLK_RIGHTPAREN] = ")";
    dnKeyNames[SDLK_ASTERISK] = "*";
    dnKeyNames[SDLK_PLUS] = "+";
    dnKeyNames[SDLK_COMMA] = ",";
    dnKeyNames[SDLK_MINUS] = "-";
    dnKeyNames[SDLK_PERIOD] = ".";
    dnKeyNames[SDLK_SLASH] = "/";
    dnKeyNames[SDLK_0] = "0";
    dnKeyNames[SDLK_1] = "1";
    dnKeyNames[SDLK_2] = "2";
    dnKeyNames[SDLK_3] = "3";
    dnKeyNames[SDLK_4] = "4";
    dnKeyNames[SDLK_5] = "5";
    dnKeyNames[SDLK_6] = "6";
    dnKeyNames[SDLK_7] = "7";
    dnKeyNames[SDLK_8] = "8";
    dnKeyNames[SDLK_9] = "9";
    dnKeyNames[SDLK_COLON] = ":";
    dnKeyNames[SDLK_SEMICOLON] = ";";
    dnKeyNames[SDLK_LESS] = "<";
    dnKeyNames[SDLK_EQUALS] = "=";
    dnKeyNames[SDLK_GREATER] = ">";
    dnKeyNames[SDLK_QUESTION] = "?";
    dnKeyNames[SDLK_AT] = "@";
    dnKeyNames[SDLK_LEFTBRACKET] = "[";
    dnKeyNames[SDLK_BACKSLASH] = "\\";
    dnKeyNames[SDLK_RIGHTBRACKET] = "]";
    //dnKeyNames[SDLK_CARET] = "SDLK_CARET";
    dnKeyNames[SDLK_UNDERSCORE] = "_";
    dnKeyNames[SDLK_BACKQUOTE] = "`";
    dnKeyNames[SDLK_a] = "A";
    dnKeyNames[SDLK_b] = "B";
    dnKeyNames[SDLK_c] = "C";
    dnKeyNames[SDLK_d] = "D";
    dnKeyNames[SDLK_e] = "E";
    dnKeyNames[SDLK_f] = "F";
    dnKeyNames[SDLK_g] = "G";
    dnKeyNames[SDLK_h] = "H";
    dnKeyNames[SDLK_i] = "I";
    dnKeyNames[SDLK_j] = "J";
    dnKeyNames[SDLK_k] = "K";
    dnKeyNames[SDLK_l] = "L";
    dnKeyNames[SDLK_m] = "M";
    dnKeyNames[SDLK_n] = "N";
    dnKeyNames[SDLK_o] = "O";
    dnKeyNames[SDLK_p] = "P";
    dnKeyNames[SDLK_q] = "Q";
    dnKeyNames[SDLK_r] = "R";
    dnKeyNames[SDLK_s] = "S";
    dnKeyNames[SDLK_t] = "T";
    dnKeyNames[SDLK_u] = "U";
    dnKeyNames[SDLK_v] = "V";
    dnKeyNames[SDLK_w] = "W";
    dnKeyNames[SDLK_x] = "X";
    dnKeyNames[SDLK_y] = "Y";
    dnKeyNames[SDLK_z] = "Z";
    dnKeyNames[SDLK_DELETE] = "DEL";
    dnKeyNames[SDLK_KP0] = "KPad 0";
    dnKeyNames[SDLK_KP1] = "KPad 1";
    dnKeyNames[SDLK_KP2] = "KPad 2";
    dnKeyNames[SDLK_KP3] = "KPad 3";
    dnKeyNames[SDLK_KP4] = "KPad 4";
    dnKeyNames[SDLK_KP5] = "KPad 5";
    dnKeyNames[SDLK_KP6] = "KPad 6";
    dnKeyNames[SDLK_KP7] = "KPad 7";
    dnKeyNames[SDLK_KP8] = "KPad 8";
    dnKeyNames[SDLK_KP9] = "KPad 9";
    dnKeyNames[SDLK_KP_PERIOD] = "KP .";
    dnKeyNames[SDLK_KP_DIVIDE] = "KP /";
    dnKeyNames[SDLK_KP_MULTIPLY] = "KP *";
    dnKeyNames[SDLK_KP_MINUS] = "KP -";
    dnKeyNames[SDLK_KP_PLUS] = "KP +";
    dnKeyNames[SDLK_KP_ENTER] = "KP Enter";
    dnKeyNames[SDLK_KP_EQUALS] = "KP =";
    dnKeyNames[SDLK_UP] = "Up";
    dnKeyNames[SDLK_DOWN] = "Down";
    dnKeyNames[SDLK_RIGHT] = "Right";
    dnKeyNames[SDLK_LEFT] = "Left";
    dnKeyNames[SDLK_INSERT] = "Inser";
    dnKeyNames[SDLK_HOME] = "Home";
    dnKeyNames[SDLK_END] = "End";
    dnKeyNames[SDLK_PAGEUP] = "PgUp";
    dnKeyNames[SDLK_PAGEDOWN] = "PgDown";
    dnKeyNames[SDLK_F1] = "F1";
    dnKeyNames[SDLK_F2] = "F2";
    dnKeyNames[SDLK_F3] = "F3";
    dnKeyNames[SDLK_F4] = "F4";
    dnKeyNames[SDLK_F5] = "F5";
    dnKeyNames[SDLK_F6] = "F6";
    dnKeyNames[SDLK_F7] = "F7";
    dnKeyNames[SDLK_F8] = "F8";
    dnKeyNames[SDLK_F9] = "F9";
    dnKeyNames[SDLK_F10] = "F10";
    dnKeyNames[SDLK_F11] = "F11";
    dnKeyNames[SDLK_F12] = "F12";
    dnKeyNames[SDLK_F13] = "F13";
    dnKeyNames[SDLK_F14] = "F14";
    dnKeyNames[SDLK_F15] = "F15";
    dnKeyNames[SDLK_NUMLOCK] = "NumLock";
    dnKeyNames[SDLK_CAPSLOCK] = "CapsLock";
    dnKeyNames[SDLK_SCROLLOCK] = "ScrlLock";
    dnKeyNames[SDLK_RSHIFT] = "RShift";
    dnKeyNames[SDLK_LSHIFT] = "LShift";
    dnKeyNames[SDLK_RCTRL] = "RCtrl";
    dnKeyNames[SDLK_LCTRL] = "LCtrl";
    dnKeyNames[SDLK_RALT] = "RAlt";
    dnKeyNames[SDLK_LALT] = "LAlt";
    dnKeyNames[SDLK_RMETA] = "LMeta";
    dnKeyNames[SDLK_LMETA] = "RMeta";
    dnKeyNames[SDLK_LSUPER] = "LSuper";
    dnKeyNames[SDLK_RSUPER] = "RSuper";
    dnKeyNames[SDLK_MODE] = "Mode";
    dnKeyNames[SDLK_COMPOSE] = "Compose";
    dnKeyNames[SDLK_HELP] = "Help";
    dnKeyNames[SDLK_PRINT] = "Print";
    dnKeyNames[SDLK_SYSREQ] = "SysRq";
    dnKeyNames[SDLK_BREAK] = "Break";
    dnKeyNames[SDLK_MENU] = "Menu";
    dnKeyNames[SDLK_POWER] = "Power";
    dnKeyNames[SDLK_EURO] = "Euro";
    dnKeyNames[SDLK_UNDO] = "Undo";
    dnKeyNames[DNK_MOUSE0] = "Mouse 1";
    dnKeyNames[DNK_MOUSE1] = "Mouse 2";
    dnKeyNames[DNK_MOUSE2] = "Mouse 3";
    dnKeyNames[DNK_MOUSE3] = "Wheel Up";
    dnKeyNames[DNK_MOUSE4] = "Wheel Dn";
    dnKeyNames[DNK_MOUSE5] = "Mouse 6";
    dnKeyNames[DNK_MOUSE6] = "Mouse 7";
    dnKeyNames[DNK_MOUSE7] = "Mouse 8";
    dnKeyNames[DNK_MOUSE8] = "Mouse 9";
    dnKeyNames[DNK_MOUSE9] = "Mouse 10";
    dnKeyNames[DNK_MOUSE10] = "Mouse 11";
    dnKeyNames[DNK_MOUSE11] = "Mouse 12";
    dnKeyNames[DNK_MOUSE12] = "Mouse 13";
    dnKeyNames[DNK_MOUSE13] = "Mouse 14";
    dnKeyNames[DNK_MOUSE14] = "Mouse 15";
    dnKeyNames[DNK_MOUSE15] = "Mouse 16";
}

void dnAssignKey(dnKey key, int action) {
    dnKeyMapping[key] = action;
}

int  dnGetKeyAction(dnKey key) {
    return dnKeyMapping[key];
}

const char* dnGetKeyName(dnKey key) {
    const char *name = dnKeyNames[key];
    if (name == NULL) {
        return "Undef";
    }
    return name;
}

dnKey dnGetKeyByName(const char *keyName) {
	int i;
    for (i = 0; i < DN_MAX_KEYS; i++) {
        const char *name = dnKeyNames[i];
        if (name != NULL && SDL_strcasecmp(keyName, name) == 0) {
            return i;
        }
    }
    return SDLK_UNKNOWN;
}

static int dnIsAutoReleaseKey(dnKey key, int function) {
    switch (key) {
        case DNK_MOUSE3:
        case DNK_MOUSE4:
            return 1;
    }
    switch (function) {
        case gamefunc_Map:
        case gamefunc_AutoRun:
        case gamefunc_Map_Follow_Mode:
        case gamefunc_Next_Track:
        case gamefunc_View_Mode:
        case gamefunc_Shrink_Screen:
        case gamefunc_Enlarge_Screen:
        case gamefunc_Mouse_Aiming:
        case gamefunc_Show_Opponents_Weapon:
        case gamefunc_Toggle_Crosshair:
        case gamefunc_See_Coop_View:
            return 1;
    }
    return 0;
}

void dnPressKey(dnKey key) {
    int function = dnKeyMapping[key];
    if (!CONTROL_CheckRange(function)) {
        DN_BUTTONSET(function, 1);
        if (dnIsAutoReleaseKey(key, function)) {
           DN_AUTORELEASESET(function, 1);
        }
    }
}

void dnReleaseKey(dnKey key) {
    int function = dnKeyMapping[key];
    if (!CONTROL_CheckRange(function)) {
        DN_BUTTONCLEAR(function);
    }
}

void dnClearKeys() {
    DN_ButtonState1 = DN_ButtonState2 = 0;
}

int  dnKeyState(dnKey key) {
    int function = dnKeyMapping[key];
    if (CONTROL_CheckRange(function)) {
        return BUTTON(function);
    }
    return 0;
}

void dnGetInput() {
    CONTROL_ButtonState1 = DN_ButtonState1;
    CONTROL_ButtonState2 = DN_ButtonState2;
    
    DN_ButtonState1 &= ~DN_AutoRelease1;
    DN_ButtonState2 &= ~DN_AutoRelease2;
    
    DN_AutoRelease1 = DN_AutoRelease2 = 0;
}

void dnResetBindings() {
    dnClearFunctionBindings();
    dnBindFunction(gamefunc_Move_Forward, 0, SDLK_w);
    dnBindFunction(gamefunc_Move_Forward, 1, SDLK_UP);
    dnBindFunction(gamefunc_Move_Backward, 0, SDLK_s);
    dnBindFunction(gamefunc_Move_Backward, 1, SDLK_DOWN);
    dnBindFunction(gamefunc_Turn_Left, 1, SDLK_LEFT);
    dnBindFunction(gamefunc_Turn_Right, 1, SDLK_RIGHT);
    dnBindFunction(gamefunc_Strafe, 0, SDLK_LALT);
    dnBindFunction(gamefunc_Strafe, 1, SDLK_RALT);
    dnBindFunction(gamefunc_Fire, 0, DNK_MOUSE0);
    dnBindFunction(gamefunc_Crouch, 0, SDLK_LCTRL);
    dnBindFunction(gamefunc_Jump, 0, SDLK_SPACE);
    dnBindFunction(gamefunc_Open, 0, SDLK_e);
    dnBindFunction(gamefunc_Open, 1, DNK_MOUSE2);
    dnBindFunction(gamefunc_Run, 0, SDLK_LSHIFT);
    dnBindFunction(gamefunc_Run, 1, SDLK_RSHIFT);
    dnBindFunction(gamefunc_AutoRun, 0, SDLK_CAPSLOCK);
    dnBindFunction(gamefunc_Strafe_Left, 0, SDLK_a);
    dnBindFunction(gamefunc_Strafe_Right, 0, SDLK_d);
    dnBindFunction(gamefunc_Aim_Up, 0, SDLK_HOME);
    dnBindFunction(gamefunc_Aim_Up, 1, SDLK_KP7);
    dnBindFunction(gamefunc_Aim_Down, 0, SDLK_END);
    dnBindFunction(gamefunc_Aim_Down, 1, SDLK_KP1);
    dnBindFunction(gamefunc_Inventory, 0, SDLK_RETURN);
    dnBindFunction(gamefunc_Inventory, 1, SDLK_KP_ENTER);
    dnBindFunction(gamefunc_Inventory_Left, 0, SDLK_LEFTBRACKET);
    dnBindFunction(gamefunc_Inventory_Right, 0, SDLK_RIGHTBRACKET);
    dnBindFunction(gamefunc_Holo_Duke, 0, SDLK_h);
    dnBindFunction(gamefunc_Jetpack, 0, SDLK_j);
    dnBindFunction(gamefunc_NightVision, 0, SDLK_n);
    dnBindFunction(gamefunc_MedKit, 0, SDLK_m);
    dnBindFunction(gamefunc_Map, 0, SDLK_TAB);
    dnBindFunction(gamefunc_Center_View, 0, SDLK_KP5);
    dnBindFunction(gamefunc_Steroids, 0, SDLK_r);
    dnBindFunction(gamefunc_Quick_Kick, 0, SDLK_COMMA);
    dnBindFunction(gamefunc_Quick_Kick, 1, SDLK_BACKQUOTE);
    dnBindFunction(gamefunc_Next_Weapon, 0, SDLK_QUOTE);
    dnBindFunction(gamefunc_Next_Weapon, 1, DNK_MOUSE3);
    dnBindFunction(gamefunc_Previous_Weapon, 0, SDLK_SEMICOLON);
    dnBindFunction(gamefunc_Previous_Weapon, 1, DNK_MOUSE4);
    
    dnBindFunction(gamefunc_Weapon_1, 0, SDLK_1);
    dnBindFunction(gamefunc_Weapon_2, 0, SDLK_2);
    dnBindFunction(gamefunc_Weapon_3, 0, SDLK_3);
    dnBindFunction(gamefunc_Weapon_4, 0, SDLK_4);
    dnBindFunction(gamefunc_Weapon_5, 0, SDLK_5);
    dnBindFunction(gamefunc_Weapon_6, 0, SDLK_6);
    dnBindFunction(gamefunc_Weapon_7, 0, SDLK_7);
    dnBindFunction(gamefunc_Weapon_8, 0, SDLK_8);
    dnBindFunction(gamefunc_Weapon_9, 0, SDLK_9);
    dnBindFunction(gamefunc_Weapon_10, 0, SDLK_0);
    
    dnBindFunction(gamefunc_Shrink_Screen, 0, SDLK_MINUS);
    dnBindFunction(gamefunc_Enlarge_Screen, 0, SDLK_EQUALS);
    dnBindFunction(gamefunc_Map_Follow_Mode, 0, SDLK_f);
    
    dnBindFunction(gamefunc_Help_Menu, 0, SDLK_F1);
    dnBindFunction(gamefunc_Save_Menu, 0, SDLK_F2);
    dnBindFunction(gamefunc_Load_Menu, 0, SDLK_F3);
    dnBindFunction(gamefunc_Sound_Menu, 0, SDLK_F4);
    dnBindFunction(gamefunc_Next_Track, 0, SDLK_F5);
    dnBindFunction(gamefunc_Quick_Save, 0, SDLK_F6);
    dnBindFunction(gamefunc_View_Mode, 0, SDLK_F7);
    dnBindFunction(gamefunc_Game_Menu, 0, SDLK_F8);
    dnBindFunction(gamefunc_Quick_Load, 0, SDLK_F9);
    dnBindFunction(gamefunc_Quit_Game, 0, SDLK_F10);
    dnBindFunction(gamefunc_Video_Menu, 0, SDLK_F11);
    
    dnBindFunction(gamefunc_Mouse_Aiming, 0, SDLK_u);
    dnBindFunction(gamefunc_SendMessage, 0, SDLK_t);
}

void dnBindFunction(int function, int slot, dnKey key) {
	int i;
    if (function >= 0 && function < 64) {
        for (i = 0; i < 64; i++) {
            if (dnFuncBindings[i][0] == key) {
                dnFuncBindings[i][0] = SDLK_UNKNOWN;
            }
            if (dnFuncBindings[i][1] == key) {
                dnFuncBindings[i][1] = SDLK_UNKNOWN;
            }
        }
        dnFuncBindings[function][slot] = key;
    }
}

SDLKey dnGetFunctionBinding(int function, int slot) {
    return dnFuncBindings[function][slot];
}

void dnClearFunctionBindings() {
    memset(dnFuncBindings, 0, sizeof(dnFuncBindings));
}

static void dnPrintBindings() {
	int i;
    for (i = 0; i < 64; i++) {
        dnKey key0 = dnFuncBindings[i][0];
        dnKey key1 = dnFuncBindings[i][1];
        dnKeyMapping[key0] = i;
        dnKeyMapping[key1] = i;
    }
}

void dnApplyBindings() {
	int i;
    memset(dnKeyMapping, -1, sizeof(dnKeyMapping));
    for (i = 0; i < 64; i++) {
        dnKey key0 = dnFuncBindings[i][0];
        dnKey key1 = dnFuncBindings[i][1];
        dnKeyMapping[key0] = i;
        dnKeyMapping[key1] = i;
    }
}

