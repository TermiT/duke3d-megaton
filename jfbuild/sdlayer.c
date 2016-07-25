// SDL interface layer
// for the Build Engine
// by Jonathon Fowler (jf@jonof.id.au)
//
// Use SDL from http://www.libsdl.org

#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "SDL.h"
#include "compat.h"
#include "sdlayer.h"
#include "cache1d.h"
#include "pragmas.h"
#include "a.h"
#include "build.h"
#include "osd.h"
#include "duke3d.h"

#ifdef USE_OPENGL
# include "glbuild.h"
# include "gui.h"
#ifdef __linux
# include <GL/glu.h>
#endif
#endif

#include "csteam.h"
#include "dnAPI.h"
#include "dnMouseInput.h"

#include "dnSnapshot.h"
#include "lz4.h"
#include "mmulti.h"
#include "log.h"
#include "crash.h"

#if 0
static snapshot_t snapshot0 = { 0 };
static snapshot_t snapshot1 = { 0 };

static int snapshot_number = 0;
#endif

int startwin_open(void) { return 0; }
int startwin_close(void) { return 0; }
int startwin_puts(const char *s) { s=s; return 0; }
int startwin_idle(void *s) { return 0; }
int startwin_settitle(const char *s) { s=s; return 0; }
int startwin_run(void) {
    return 1;
}

#define GP_GET_AXIS(AXIS)  SDL_GameControllerGetAxis(gamepad, AXIS)
#define GP_GET_BUTTON(BUTTON) SDL_GameControllerGetButton(gamepad, BUTTON)
#define GP_AXIS_DEADZONE   0.27
#define GP_AXIS_MAX_VALUE  32767

#define SURFACE_FLAGS	(SDL_SWSURFACE|SDL_HWPALETTE|SDL_HWACCEL)

// undefine to restrict windowed resolutions to conventional sizes
#define ANY_WINDOWED_SIZE

int   _buildargc = 1;
const char **_buildargv = NULL;
extern long app_main(long argc, char *argv[]);

char quitevent=0, appactive=1;

static SDL_Surface *sdl_buffersurface=NULL;
static SDL_Palette *sdl_palptr=NULL;
// static SDL_Window *sdl_window=NULL;
static SDL_GLContext sdl_context=NULL;
long xres=-1, yres=-1, bpp=0, fullscreen=0, bytesperline, imageSize;
long frameplace=0, lockcount=0;
char modechange=1;
char offscreenrendering=0;
char videomodereset = 0;
static unsigned short sysgamma[3][256];
extern long curbrightness, gammabrightness;

#ifdef USE_OPENGL
// OpenGL stuff
static char nogl=0;
#endif

// input
char inputdevices=0;
char keystatus[256], keyfifo[KEYFIFOSIZ], keyfifoplc, keyfifoend;
unsigned char keyasciififo[KEYFIFOSIZ], keyasciififoplc, keyasciififoend;
static unsigned char keynames[256][24];
long mousex=0,mousey=0,mouseb=0;
long *joyaxis = NULL, joyb=0, *joyhat = NULL;
char joyisgamepad=0, joynumaxes=0, joynumbuttons=0, joynumhats=0;
long joyaxespresent=0;
int gamepadButtonPressed = 0;

void (*keypresscallback)(long,long) = 0;
void (*mousepresscallback)(long,long) = 0;
void (*joypresscallback)(long,long) = 0;

static char keytranslation[SDL_NUM_SCANCODES];
static int buildkeytranslationtable(void);

//static SDL_Surface * loadtarga(const char *fn);		// for loading the icon
static SDL_Surface * loadappicon(void);


int osx_msgbox(char* name, char* buf){
    return 1;
}

int osx_ynbox(char* name, char* buf){
    return 1;
}

#ifdef _WIN32
extern int Win32_ShowErrorMessage(const char *, const char *);
#endif

int wm_msgbox(char *name, char *fmt, ...)
{
	char buf[1000];
	va_list va;

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);
	
	Log_Puts( buf );
	Log_Puts( "\n" );

#if defined(__APPLE__)
	return osx_msgbox(name, buf);
#elif defined HAVE_GTK2
	if (gtkbuild_msgbox(name, buf) >= 0) return 1;
#elif defined _WIN32
	return Win32_ShowErrorMessage(name, buf);
#else
    // Replace all tab chars with spaces because the hand-rolled SDL message
    // box diplays the former as N/L instead of whitespace.
    {
        int32_t i;
        for (i=0; i<(int32_t)sizeof(buf); i++)
            if (buf[i] == '\t')
                buf[i] = ' ';
    }
    return SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, name, buf, NULL);
#endif

	return 0;
}

int wm_ynbox(char *name, char *fmt, ...)
{
	char buf[1000];
	char c;
	va_list va;

	va_start(va,fmt);
	vsprintf(buf,fmt,va);
	va_end(va);

#if defined __APPLE__
	return osx_ynbox(name, buf);
#elif defined HAVE_GTK2
	if ((r = gtkbuild_ynbox(name, buf)) >= 0) return r;
#endif
	puts(buf);
	puts("   (type 'Y' or 'N', and press Return or Enter to continue)");
	do c = getchar(); while (c != 'Y' && c != 'y' && c != 'N' && c != 'n');
	if (c == 'Y' || c == 'y') return 1;

	return 0;
}

void wm_setapptitle(char *name)
{
	if (name) {
		Bstrncpy(apptitle, name, sizeof(apptitle)-1);
		apptitle[ sizeof(apptitle)-1 ] = 0;
	}
    SDL_SetWindowTitle(sdl_window, apptitle);
	startwin_settitle(apptitle);
}


//
//
// ---------------------------------------
//
// System
//
// ---------------------------------------
//
//

void app_atexit( void ) {
	CrashHandler_Free();
}

int main(int argc, char *argv[])
{
	int r;
	
	CrashHandler_Init();
	
	atexit( &app_atexit );
	
	if (!CSTEAM_Init()) {
		wm_msgbox("Error", "Could not initialize Steam. Please check that your Steam client is up and running.");
		return -1;
	}
	buildkeytranslationtable();
	
#ifdef HAVE_GTK2
	gtkbuild_init(&argc, &argv);
#endif
	startwin_open();

	_buildargc = argc;
	_buildargv = (const char **)argv;
    
	baselayer_init();
	r = app_main(argc, argv);

	startwin_close();
#ifdef HAVE_GTK2
	gtkbuild_exit(r);
#endif
	return r;
}

//
// initsystem() -- init SDL systems
//
int initsystem(void)
{
	SDL_version compiled;

    SDL_version linked_;
    const SDL_version *linked = &linked_;
    SDL_GetVersion(&linked_);

	SDL_VERSION(&compiled);

	initprintf("Initialising SDL system interface "
		  "(compiled with SDL version %d.%d.%d, DLL version %d.%d.%d)\n",
		linked->major, linked->minor, linked->patch,
		compiled.major, compiled.minor, compiled.patch);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER
#ifdef NOSDLPARACHUTE
			| SDL_INIT_NOPARACHUTE
#endif
		)) {
		initprintf("Initialisation failed! (%s)\n", SDL_GetError());
		return -1;
	}

    grabmouse(1);
	atexit(uninitsystem);

	frameplace = 0;
	lockcount = 0;

#ifdef USE_OPENGL
	if (loadgldriver(getenv("BUILD_GLDRV"))) {
//		initprintf("Failed loading OpenGL driver. GL modes will be unavailable.\n");
//		nogl = 1;
        initprintf("Failed loading OpenGL driver. Exiting...\n");
        return 1;
	}
#endif

/*
#ifndef __APPLE__
	{
		SDL_Surface *icon;
		//icon = loadtarga("icon.tga");
		icon = loadappicon();
		if (icon) {
            SDL_SetWindowIcon(sdl_window, icon);
		}
	}
#endif
*/

    {
        const char *drvname = SDL_GetVideoDriver(0);
        if (drvname)
            initprintf("Using \"%s\" video driver\n", drvname);
    }

	// dump a quick summary of the graphics hardware
#ifdef DEBUGGINGAIDS
	vid = SDL_GetVideoInfo();
	initprintf("Video device information:\n");
	initprintf("  Can create hardware surfaces?          %s\n", (vid->hw_available)?"Yes":"No");
	initprintf("  Window manager available?              %s\n", (vid->wm_available)?"Yes":"No");
	initprintf("  Accelerated hardware blits?            %s\n", (vid->blit_hw)?"Yes":"No");
	initprintf("  Accelerated hardware colourkey blits?  %s\n", (vid->blit_hw_CC)?"Yes":"No");
	initprintf("  Accelerated hardware alpha blits?      %s\n", (vid->blit_hw_A)?"Yes":"No");
	initprintf("  Accelerated software blits?            %s\n", (vid->blit_sw)?"Yes":"No");
	initprintf("  Accelerated software colourkey blits?  %s\n", (vid->blit_sw_CC)?"Yes":"No");
	initprintf("  Accelerated software alpha blits?      %s\n", (vid->blit_sw_A)?"Yes":"No");
	initprintf("  Accelerated colour fills?              %s\n", (vid->blit_fill)?"Yes":"No");
	initprintf("  Total video memory:                    %dKB\n", vid->video_mem);
#endif

	return 0;
}


//
// uninitsystem() -- uninit SDL systems
//
void uninitsystem(void)
{
	uninitinput();
	uninitmouse();
	uninittimer();

    

	SDL_Quit();

#ifdef USE_OPENGL
	unloadgldriver();
#endif
}


//
// initprintf() -- prints a string to the intitialization window
//
void initprintf(const char *f, ...)
{
	va_list va;
	char buf[1024];
	
#if 0
	va_start(va, f);
	vprintf(f,va);
	va_end(va);
#endif
	
	va_start(va, f);
	Bvsnprintf(buf, 1024, f, va);
	va_end(va);
	OSD_Printf(buf);

	Log_Puts( buf );
	startwin_puts(buf);
	startwin_idle(NULL);
}


//
// debugprintf() -- prints a debug string to stderr
//
void debugprintf(const char *f, ...)
{
#ifdef DEBUGGINGAIDS
	va_list va;

	va_start(va,f);
	Bvfprintf(stderr, f, va);
	va_end(va);
#endif
}


//
//
// ---------------------------------------
//
// All things Input
//
// ---------------------------------------
//
//

static char mouseacquired=0,moustat=0;
SDL_GameController *gamepad = NULL;
SDL_Haptic *haptic = NULL;
void closegamepad(void) {
    if (gamepad != NULL) {
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
    }    
}

void getgamepad(void) {
	int i;
    extern int nogamepad;
    if (nogamepad) return;
    closegamepad();
    for (i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            gamepad = SDL_GameControllerOpen(i);
            if (gamepad) {
                char const * name = SDL_GameControllerName(gamepad);
                printf("Gamepad found \"%s\"\n", name);
                break;
            } else {
                fprintf(stderr, "Could not open game gamepad %i: %s\n", i, SDL_GetError());
            }
        }
    }
    if (gamepad != NULL) {
        if (SDL_NumHaptics() > 0) {
            haptic = SDL_HapticOpen(0);
            if (SDL_HapticRumbleSupported(haptic)) {
                SDL_HapticRumbleInit(haptic);
            } else {
                SDL_HapticClose(haptic);
                haptic = NULL;
            }

        }
    }
}


//Temporary replacment for SDL_GameControllerAddMappingsFromRW for 2.0.1
#define GameControllerAddMappingsFromFile(file)   GameControllerAddMappingsFromRW(SDL_RWFromFile(file, "rb"), 1)
#define CONTROLLER_PLATFORM_FIELD "platform:"
int GameControllerAddMappingsFromRW( SDL_RWops * rw, int freerw )
{
    const char *platform = SDL_GetPlatform();
    int controllers = 0;
    char *buf, *line, *line_end, *tmp, *comma, line_platform[64];
    size_t db_size, platform_len;
    
    if (rw == NULL) {
        return SDL_SetError("Invalid RWops");
    }
    db_size = (size_t)SDL_RWsize(rw);
    
    buf = (char *)SDL_malloc(db_size + 1);
    if (buf == NULL) {
        if (freerw) {
            SDL_RWclose(rw);
        }
        return SDL_SetError("Could allocate space to not read DB into memory");
    }
    
    if (SDL_RWread(rw, buf, db_size, 1) != 1) {
        if (freerw) {
            SDL_RWclose(rw);
        }
        SDL_free(buf);
        return SDL_SetError("Could not read DB");
    }
    
    if (freerw) {
        SDL_RWclose(rw);
    }
    
    buf[db_size] = '\0';
    line = buf;
    
    while (line < buf + db_size) {
        line_end = SDL_strchr( line, '\n' );
        if (line_end != NULL) {
            *line_end = '\0';
        }
        else {
            line_end = buf + db_size;
        }
        
        /* Extract and verify the platform */
        tmp = SDL_strstr(line, CONTROLLER_PLATFORM_FIELD);
        if ( tmp != NULL ) {
            tmp += SDL_strlen(CONTROLLER_PLATFORM_FIELD);
            comma = SDL_strchr(tmp, ',');
            if (comma != NULL) {
                platform_len = comma - tmp + 1;
                if (platform_len + 1 < SDL_arraysize(line_platform)) {
                    SDL_strlcpy(line_platform, tmp, platform_len);
                    if(SDL_strncasecmp(line_platform, platform, platform_len) == 0
                       && SDL_GameControllerAddMapping(line) > 0) {
                        controllers++;
                    }
                }
            }
        }
        
        line = line_end + 1;
    }
    
    SDL_free(buf);
    return controllers;
}



//
// initinput() -- init input system
//


int initinput(void)
{
	int i;
	
#ifdef __APPLE__
	// force OS X to operate in >1 button mouse mode so that LMB isn't adulterated
	if (!getenv("SDL_HAS3BUTTONMOUSE")) putenv("SDL_HAS3BUTTONMOUSE=1");
#endif
	
	inputdevices = 1|2;	// keyboard (1) and mouse (2)
	mouseacquired = 0;

	memset(keynames,0,sizeof(keynames));

    for (i=0; i<SDL_NUM_SCANCODES; i++)
    {
        if (!keytranslation[i]) continue;
        strncpy((char *)keynames[ keytranslation[i] ], SDL_GetKeyName(SDL_SCANCODE_TO_KEYCODE(i)), sizeof(keynames[i]));
    }    
    
    SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
    GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
    
#if defined( _WIN32 )
    SDL_GameControllerAddMapping("88880803000000000000504944564944,PS3 Controller,a:b2,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b9,lefttrigger:b6,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:b7,rightx:a3,righty:a4,start:b11,x:b3,y:b0,platform:Windows");
    SDL_GameControllerAddMapping("4c056802000000000000504944564944,PS3 Controller,a:b14,b:b13,back:b0,dpdown:b7,dpleft:b8,dpright:b5,dpup:b4,guide:b16,leftshoulder:b10,leftstick:b1,lefttrigger:b8,leftx:a0,lefty:a1,rightshoulder:b11,rightstick:b2,righttrigger:b9,rightx:a2,righty:a3,start:b3,x:b12,y:b15,platform:Windows");
    SDL_GameControllerAddMapping("25090500000000000000504944564944,PS3 DualShock,a:b2,b:b1,back:b9,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:,leftshoulder:b6,leftstick:b10,lefttrigger:b4,leftx:a0,lefty:a1,rightshoulder:b7,rightstick:b11,righttrigger:b5,rightx:a2,righty:a3,start:b8,x:b3,y:b0,platform:Windows");
#endif
    getgamepad();
	return 0;
}

//
// uninitinput() -- uninit input system
//
void uninitinput(void)
{
	uninitmouse();
	if(haptic != NULL) {
		SDL_HapticClose(haptic);
	}
//    if (gamepad != NULL) {
//        SDL_GameControllerClose(gamepad);
//    }
}

const unsigned char *getkeyname(int num)
{
	if ((unsigned)num >= 256) return NULL;
	return keynames[num];
}

const unsigned char *getjoyname(int what, int num)
{
	static char tmp[64];

	switch (what) {
		case 0:	// axis
			if ((unsigned)num > (unsigned)joynumaxes) return NULL;
			sprintf(tmp,"Axis %d",num);
			return (unsigned char *)tmp;

		case 1: // button
			if ((unsigned)num > (unsigned)joynumbuttons) return NULL;
			sprintf(tmp,"Button %d",num);
			return (unsigned char *)tmp;

		case 2: // hat
			if ((unsigned)num > (unsigned)joynumhats) return NULL;
			sprintf(tmp,"Hat %d",num);
			return (unsigned char *)tmp;

		default:
			return NULL;
	}
}

//
// bgetchar, bkbhit, bflushchars -- character-based input functions
//
unsigned char bgetchar(void)
{
	unsigned char c;
	if (keyasciififoplc == keyasciififoend) return 0;
	c = keyasciififo[keyasciififoplc];
	keyasciififoplc = ((keyasciififoplc+1)&(KEYFIFOSIZ-1));
	return c;
}

int bkbhit(void)
{
	return (keyasciififoplc != keyasciififoend) || gamepadButtonPressed;
}

void bflushchars(void)
{
	keyasciififoplc = keyasciififoend = gamepadButtonPressed = 0;
}


//
// set{key|mouse|joy}presscallback() -- sets a callback which gets notified when keys are pressed
//
void setkeypresscallback(void (*callback)(long, long)) { keypresscallback = callback; }
void setmousepresscallback(void (*callback)(long, long)) { mousepresscallback = callback; }
void setjoypresscallback(void (*callback)(long, long)) { joypresscallback = callback; }

//
// initmouse() -- init mouse input
//
int initmouse( void ) {
    dnInitMouseInput();
	moustat = 1;
	grabmouse( 1 );
	return 0;
}

//
// uninitmouse() -- uninit mouse input
//
void uninitmouse( void ) {
	grabmouse( 0 );
	moustat = 0;
    dnShutdownMouseInput();
}

void dnGrabMouse( int grab ) {
    SDL_SetWindowGrab( sdl_window, (SDL_bool)grab );
#if !defined( _WIN32 )
    SDL_SetRelativeMouseMode( (SDL_bool)grab );
#else
    if ( grab ) {
        if ( dnGetNumberOfMouseInputDevices() == 0 ) {
            SDL_SetRelativeMouseMode( SDL_TRUE );
        }
    } else {
        SDL_SetRelativeMouseMode( SDL_FALSE );
    }
#endif
}

//
// grabmouse() -- show/hide mouse cursor
//
void grabmouse(char a) {
    mouseacquired = 1;
    mousex = mousey = 0;
}

//
// readmousexy() -- return mouse motion information
//
void readmousexy(long *x, long *y)
{
	if (!mouseacquired || !appactive || !moustat) {
        *x = *y = 0;
        return;
    }
	*x = mousex;
	*y = mousey;
	mousex = mousey = 0;
}

//
// readmousebstatus() -- return mouse button information
//
void readmousebstatus(long *b)
{
	if (!mouseacquired || !appactive || !moustat) *b = 0;
	else {
        *b = mouseb;
    }
}

//
// setjoydeadzone() -- sets the dead and saturation zones for the joystick
//
void setjoydeadzone(int axis, unsigned short dead, unsigned short satur)
{
}


//
// getjoydeadzone() -- gets the dead and saturation zones for the joystick
//
void getjoydeadzone(int axis, unsigned short *dead, unsigned short *satur)
{
	*dead = *satur = 0;
}


//
// releaseallbuttons()
//
void releaseallbuttons(void)
{
}


//
//
// ---------------------------------------
//
// All things Timer
// Ken did this
//
// ---------------------------------------
//
//

void Sys_InitTimer();
void Sys_UninitTimer();
double Sys_GetTicks();

static 
Uint32 GetTicks() {
	return (Uint32)Sys_GetTicks();
}

#define SDL_GetTicks() GetTicks()

static Uint32 timerfreq=0;
static Uint32 timerlastsample=0;
static Uint32 timerticspersec=0;
static void (*usertimercallback)(void) = NULL;

//
// inittimer() -- initialise timer
//
int inittimer(int tickspersecond)
{
	if (timerfreq) return 0;	// already installed

	Sys_InitTimer();

	initprintf("Initialising timer\n");

	timerfreq = 1000;
	timerticspersec = tickspersecond;
	timerlastsample = SDL_GetTicks() * timerticspersec / timerfreq;

	usertimercallback = NULL;

	return 0;
}

//
// uninittimer() -- shut down timer
//
void uninittimer(void)
{
	if (!timerfreq) return;
	
	Sys_UninitTimer();

	timerfreq=0;
}

//
// sampletimer() -- update totalclock
//
void sampletimer(void)
{
	Uint32 i;
	long n;
	
	if (!timerfreq) return;
	
	i = SDL_GetTicks();
	n = (long)(i * timerticspersec / timerfreq) - timerlastsample;
	if (n>0) {
		totalclock += n;
		timerlastsample += n;
	}

	if (usertimercallback) for (; n>0; n--) usertimercallback();
}

//
// getticks() -- returns a millisecond ticks count
//
unsigned long getticks(void)
{
	return (unsigned long)SDL_GetTicks();
}

//
// getusecticks() -- returns a microsecond ticks count
//
unsigned long getusecticks(void)
{
	return (unsigned long)(Sys_GetTicks() * 1000.0);
}


//
// gettimerfreq() -- returns the number of ticks per second the timer is configured to generate
//
int gettimerfreq(void)
{
	return timerticspersec;
}


//
// installusertimercallback() -- set up a callback function to be called when the timer is fired
//
void (*installusertimercallback(void (*callback)(void)))(void)
{
	void (*oldtimercallback)(void);

	oldtimercallback = usertimercallback;
	usertimercallback = callback;

	return oldtimercallback;
}



//
//
// ---------------------------------------
//
// All things Video
//
// ---------------------------------------
//
// 


//
// getvalidmodes() -- figure out what video modes are available
//
static int sortmodes(const struct validmode_t *a, const struct validmode_t *b)
{
	int x;

	if ((x = a->fs   - b->fs)   != 0) return x;
	if ((x = a->bpp  - b->bpp)  != 0) return x;
	if ((x = a->xdim - b->xdim) != 0) return x;
	if ((x = a->ydim - b->ydim) != 0) return x;

	return 0;
}

static char modeschecked=0;

#if 1

static
int addmode( int w, int h, int bpp, int fs ) {
	int i;
	struct validmode_t *mode;
	for ( i = 0; i < validmodecnt; i++ ) {
		mode = &validmode[i];
		if ( mode->xdim == w && mode->ydim == h && mode->bpp == bpp && mode->fs == fs ) {
			break;
		}
	}
	if ( i == validmodecnt ) {
		mode = &validmode[i];
		memset( (void*)mode, 0, sizeof( struct validmode_t ) );
		mode->xdim = w;
		mode->ydim = h;
		mode->bpp = bpp;
		mode->fs = fs;
		validmodecnt++;
		return 1;
	}
	return 0;
}

struct DisplaySize {
	int w, h;
};

void getvalidmodes( void ) {
    struct DisplaySize defaultres[] = { { 0, 0 },
        {1920, 1440}, {1920, 1200}, {1920, 1080}, {1680, 1050}, {1600, 1200}, {1600, 900}, {1440, 1080},
        {1366, 768}, {1360, 768}, {1280, 1024}, {1280, 960}, {1280, 800}, {1152, 864}, {1024, 768}, {1024, 600}, {800, 600},
        {800, 480}, {640, 480}
    };
	int i, nummodes;
	SDL_DisplayMode mode, closest;
	SDL_Window *window = SDL_CreateWindow( "", 0, 0, 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS );
	int displayIndex = SDL_GetWindowDisplayIndex( window );
	SDL_DestroyWindow( window );
		
	if ( !modeschecked ) {
		validmodecnt = 0;
		SDL_GetDesktopDisplayMode( displayIndex, &mode );
		defaultres[0].w = mode.w;
		defaultres[0].h = mode.h;
#if 0
		
		for ( i = 0; i < sizeof( defaultres ) / sizeof( defaultres[0] ); i++ ) {
			memset( (void*)&mode, 0, sizeof( SDL_DisplayMode ) );
			mode.w = defaultres[i].w;
			mode.h = defaultres[i].h;
			if ( SDL_GetClosestDisplayMode( displayIndex, &mode, &closest ) != NULL ) {
				addmode( mode.w, mode.h, 32, 1 );
				if ( mode.w < defaultres[0].w ) {
					addmode( mode.w, mode.h, 32, 0 );
				}
			}
		}
#else
		nummodes = SDL_GetNumDisplayModes( displayIndex );
		for ( i = 0 ; i < nummodes; i++ ) {
			memset( (void*)&mode, 0, sizeof( SDL_DisplayMode ) );
			SDL_GetDisplayMode( displayIndex, i, &mode );
			addmode( mode.w, mode.h, 32, 1 );
			if ( mode.w < defaultres[0].w ) {
				addmode( mode.w, mode.h, 32, 0 );
			}
		}
#endif
		modeschecked = 1;
	}
}
#else
void getvalidmodes(void)
{
    int32_t i, maxx=0, maxy=0;

    int defaultres[][2] =
    {
        {1920, 1440}, {1920, 1200}, {1920, 1080}, {1680, 1050}, {1600, 1200}, {1600, 900}, {1440, 1080},
        {1366, 768}, {1360, 768}, {1280, 1024}, {1280, 960}, {1152, 864}, {1024, 768}, {1024, 600}, {800, 600},
        {640, 480} /*, {640, 400}, {512, 384}, {480, 360}, {400, 300}, {320, 240}, {320, 200}, {0, 0} */,
    };

    SDL_DisplayMode dispmode;

    if (modeschecked) return;

    validmodecnt=0;
//    initprintf("Detecting video modes:\n");

#define ADDMODE(x,y,c,f) do { \
    if (validmodecnt<MAXVALIDMODES) { \
        int32_t mn; \
        for(mn=0;mn<validmodecnt;mn++) \
            if (validmode[mn].xdim==x && validmode[mn].ydim==y && \
                validmode[mn].bpp==c  && validmode[mn].fs==f) break; \
        if (mn==validmodecnt) { \
            validmode[validmodecnt].xdim=x; \
            validmode[validmodecnt].ydim=y; \
            validmode[validmodecnt].bpp=c; \
            validmode[validmodecnt].fs=f; \
            validmodecnt++; \
            /*initprintf("  - %dx%d %d-bit %s\n", x, y, c, (f&1)?"fullscreen":"windowed");*/ \
        } \
    } \
} while (0)

#define CHECK(w,h) if ((w < maxx) && (h < maxy))

    // do fullscreen modes first
    for (i=0; i<SDL_GetNumDisplayModes(0); i++)
    {
        SDL_GetDisplayMode(0, i, &dispmode);
        if ((dispmode.w > MAXXDIM) || (dispmode.h > MAXYDIM)) continue;

        // HACK: 8-bit == Software, 32-bit == OpenGL
#ifdef USE_SWRENDER
        ADDMODE(dispmode.w, dispmode.h, 8, 1);
#endif
#ifdef USE_OPENGL
        if (!nogl)
            ADDMODE(dispmode.w, dispmode.h, 32, 1);
#endif
        if ((dispmode.w > maxx) && (dispmode.h > maxy))
        {
            maxx = dispmode.w;
            maxy = dispmode.h;
        }
    }
    if (maxx == 0 && maxy == 0)
    {
        initprintf("No fullscreen modes available!\n");
        maxx = MAXXDIM; maxy = MAXYDIM;
    }

    // add windowed modes next
    for (i=0; defaultres[i][0]; i++)
        CHECK(defaultres[i][0],defaultres[i][1])
        {
            // HACK: 8-bit == Software, 32-bit == OpenGL
#ifdef USE_SWRENDER
            ADDMODE(defaultres[i][0],defaultres[i][1],8,0);
#endif
#ifdef USE_OPENGL
            if (!nogl)
                ADDMODE(defaultres[i][0],defaultres[i][1],32,0);
#endif
        }

#undef CHECK
#undef ADDMODE

    qsort((void *)validmode, validmodecnt, sizeof(struct validmode_t), &sortmodes);

    modeschecked=1;
}
#endif

//
// checkvideomode() -- makes sure the video mode passed is legal
//
int checkvideomode(int *x, int *y, int c, int fs, int forced)
{
	int i, nearest=-1, dx, dy, odx=9999, ody=9999;

	getvalidmodes();

	if (c>8
#ifdef USE_OPENGL
	    && nogl
#endif
	   ) return -1;

	// fix up the passed resolution values to be multiples of 8
	// and at least 320x200 or at most MAXXDIMxMAXYDIM
	if (*x < 320) *x = 320;
	if (*y < 200) *y = 200;
	if (*x > MAXXDIM) *x = MAXXDIM;
	if (*y > MAXYDIM) *y = MAXYDIM;
	*x &= 0xfffffff8l;

	for (i=0; i<validmodecnt; i++) {
		if (validmode[i].bpp != c) continue;
		if (validmode[i].fs != fs) continue;
		dx = klabs(validmode[i].xdim - *x);
		dy = klabs(validmode[i].ydim - *y);
		if (!(dx | dy)) { 	// perfect match
			nearest = i;
			break;
		}
		if ((dx <= odx) && (dy <= ody)) {
			nearest = i;
			odx = dx; ody = dy;
		}
	}
		
#ifdef ANY_WINDOWED_SIZE
	if (!forced && (fs&1) == 0 && (nearest < 0 || (validmode[nearest].xdim!=*x || validmode[nearest].ydim!=*y)))
		return 0x7fffffffl;
#endif

	if (nearest < 0) {
		// no mode that will match (eg. if no fullscreen modes)
		return -1;
	}

	*x = validmode[nearest].xdim;
	*y = validmode[nearest].ydim;

	return nearest;		// JBF 20031206: Returns the mode number
}

static SDL_Color sdlayer_pal[256];

static void destroy_window_resources() {
	if ( sdl_buffersurface ) {
		SDL_FreeSurface( sdl_buffersurface );
	}
	sdl_buffersurface = NULL;
	if ( sdl_window ) {
		SDL_DestroyWindow( sdl_window );
	}
	sdl_window = NULL;
}

//
// setvideomode() -- set SDL video mode
//

int setvideomode_new( int x, int y, int c, int fs, int force ) {
	int regrab = 0;
	Uint32 flags;
	
	initprintf( "[sdlayer] Setting video mode %dx%d@%d, %s%s", x, y, c, fs ? "fullscreen" : "windowed", force ? " (force)" : "" );
	
	if ( ( fs == fullscreen ) && ( x == xres ) && ( y == yres)  && ( c == bpp ) && !videomodereset && !force ) {
		initprintf( "[sdlayer] No need to change mode, we're already there\n" );
		OSD_ResizeDisplay( xres, yres );
		return 0;
	}
	
	if ( checkvideomode( &x, &y, c, fs, 0 ) < 0 ) {
		initprintf( "[sdlayer] Wrong mode\n" );
		return -1;
	}
	
	if ( mouseacquired ) {
		regrab = 1;
		grabmouse( 0 );
	}

	while ( lockcount ) {
		enddrawing();
	}
	
	polymost_glreset();
	
	if ( sdl_window == NULL ) {
		flags = SDL_WINDOW_OPENGL;
		if ( fs ) {
			flags |= SDL_WINDOW_FULLSCREEN;
		} else {
			flags |= SDL_WINDOW_SHOWN;
		}
		sdl_window = SDL_CreateWindow( "", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x, y, flags );
		sdl_context = SDL_GL_CreateContext( sdl_window );
		
		
		
	} else {
		GUI_PreModeChange();

		SDL_SetWindowFullscreen( sdl_window, 0 );
		SDL_SetWindowSize( sdl_window, x, y );
		SDL_SetWindowPosition( sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );
		
		if ( fs ) {
			SDL_SetWindowFullscreen( sdl_window, SDL_WINDOW_FULLSCREEN );
			SDL_SetWindowDisplayMode( sdl_window, NULL );
		}
		
		GUI_PostModeChange( x, y );
	}
}

int setvideomode(int x, int y, int c, int fs, int force)
{
	int regrab = 0;
	SDL_DisplayMode displayMode = { 0 };
	Uint16 rg, gg, bg;
	Uint32 flags;
	
	if ((fs == fullscreen) && (x == xres) && (y == yres) && (c == bpp) &&
	    !videomodereset && !force) {
		OSD_ResizeDisplay(xres,yres);
		return 0;
	}

	if (checkvideomode(&x,&y,c,fs,0) < 0) return -1;

	startwin_close();

	if (mouseacquired) {
		regrab = 1;
		grabmouse(0);
	}

	if (lockcount) while (lockcount) enddrawing();

#if defined(USE_OPENGL)
	polymost_glreset();
#endif

//	if ( sdl_window != NULL ) {
//		SDL_SetWindowGammaRamp( sdl_window, sysgamma[0], sysgamma[1], sysgamma[2] );
//	}

    // deinit
    //destroy_window_resources();
	
#if defined(USE_OPENGL)
	if (c > 8) {
		int i, j, multisamplecheck = (glmultisample > 0);
		struct {
			SDL_GLattr attr;
			int value;
		} attributes[] = {
#if 1
			{ SDL_GL_RED_SIZE, 8 },
			{ SDL_GL_GREEN_SIZE, 8 },
			{ SDL_GL_BLUE_SIZE, 8 },
			{ SDL_GL_ALPHA_SIZE, 8 },
			{ SDL_GL_BUFFER_SIZE, c },
			{ SDL_GL_STENCIL_SIZE, 0 },
			{ SDL_GL_ACCUM_RED_SIZE, 0 },
			{ SDL_GL_ACCUM_GREEN_SIZE, 0 },
			{ SDL_GL_ACCUM_BLUE_SIZE, 0 },
			{ SDL_GL_ACCUM_ALPHA_SIZE, 0 },
			{ SDL_GL_DEPTH_SIZE, 24 },
#endif
			{ SDL_GL_DOUBLEBUFFER, 1 },
			{ SDL_GL_MULTISAMPLEBUFFERS, glmultisample > 0 },
			{ SDL_GL_MULTISAMPLESAMPLES, glmultisample },
		};

		if (nogl) return -1;
	initprintf("Setting video mode %dx%d (%d-bpp %s)\n",
	
				x,y,c, ((fs&1) ? "fullscreen" : "windowed"));
		do {
			if ( sdl_window == NULL ) {
				flags = SDL_WINDOW_OPENGL;
				if ( fs ) {
					flags |= SDL_WINDOW_FULLSCREEN;
				} else {
					flags |= SDL_WINDOW_SHOWN;
				}
				sdl_window = SDL_CreateWindow( "", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x, y, flags );
				sdl_context = SDL_GL_CreateContext( sdl_window );				
			} else {
				GUI_PreModeChange();
				SDL_SetWindowFullscreen( sdl_window, 0 );
				SDL_SetWindowSize( sdl_window, x, y );
				SDL_SetWindowPosition( sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );
				
				if ( fs ) {
					SDL_SetWindowFullscreen( sdl_window, SDL_WINDOW_FULLSCREEN );
					SDL_SetWindowDisplayMode( sdl_window, NULL );
				}
				
				GUI_PostModeChange( x, y );
			}
			
			SDL_GL_SetSwapInterval( ud.vsync );
			
			for (i=0; i < (int)(sizeof(attributes)/sizeof(attributes[0])); i++) {
				j = attributes[i].value;
				if (!multisamplecheck &&
				    (attributes[i].attr == SDL_GL_MULTISAMPLEBUFFERS ||
				     attributes[i].attr == SDL_GL_MULTISAMPLESAMPLES)
					) {
					j = 0;
				}
				SDL_GL_SetAttribute(attributes[i].attr, j);
			}
			
			dnSetBrightness( ud.brightness );

		} while (multisamplecheck--);
	} else
#endif
	{
		initprintf("Setting video mode %dx%d (%d-bpp %s)\n",
				x,y,c, ((fs&1) ? "fullscreen" : "windowed"));

        // init
        sdl_window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
                                      x,y, ((fs&1)?SDL_WINDOW_FULLSCREEN:0));
        if (!sdl_window)
        {
            initprintf("Unable to set video mode: SDL_CreateWindow failed: %s\n",
                       SDL_GetError());
            return -1;
        }

        sdl_buffersurface = SDL_CreateRGBSurface(0, x, y, c, 0, 0, 0, 0);
        if (!sdl_buffersurface)
        {
            initprintf("Unable to set video mode: SDL_CreateRGBSurface failed: %s\n",
                       SDL_GetError());
            destroy_window_resources();
            return -1;
        }

        if (!sdl_palptr)
            sdl_palptr = SDL_AllocPalette(256);

        if (SDL_SetSurfacePalette(sdl_buffersurface, sdl_palptr) < 0)
            initprintf("SDL_SetSurfacePalette failed: %s\n", SDL_GetError());
	}

#if 0
	{
	char flags[512] = "";
#define FLAG(x,y) if ((sdl_surface->flags & x) == x) { strcat(flags, y); strcat(flags, " "); }
	FLAG(SDL_HWSURFACE, "HWSURFACE") else
	FLAG(SDL_SWSURFACE, "SWSURFACE")
	FLAG(SDL_ASYNCBLIT, "ASYNCBLIT")
	FLAG(SDL_ANYFORMAT, "ANYFORMAT")
	FLAG(SDL_HWPALETTE, "HWPALETTE")
	FLAG(SDL_DOUBLEBUF, "DOUBLEBUF")
	FLAG(SDL_FULLSCREEN, "FULLSCREEN")
	FLAG(SDL_OPENGL, "OPENGL")
	FLAG(SDL_OPENGLBLIT, "OPENGLBLIT")
	FLAG(SDL_RESIZABLE, "RESIZABLE")
	FLAG(SDL_HWACCEL, "HWACCEL")
	FLAG(SDL_SRCCOLORKEY, "SRCCOLORKEY")
	FLAG(SDL_RLEACCEL, "RLEACCEL")
	FLAG(SDL_SRCALPHA, "SRCALPHA")
	FLAG(SDL_PREALLOC, "PREALLOC")
#undef FLAG
	initprintf("SDL Surface flags: %s\n", flags);
	}
#endif

	{
		//static char t[384];
		//sprintf(t, "%s (%dx%d %s)", apptitle, x, y, ((fs) ? "fullscreen" : "windowed"));
        SDL_SetWindowTitle(sdl_window, apptitle);
	}

#ifdef USE_OPENGL
	if (c > 8) {
		polymost_glreset();
		baselayer_setupopengl();
	}
#endif
	
	xres = x;
	yres = y;
	bpp = c;
	fullscreen = fs;
	//bytesperline = sdl_surface->pitch;
	//imageSize = bytesperline*yres;
	numpages = c > 8 ? 2 : 1;
	frameplace = 0;
	lockcount = 0;
	modechange=1;
	videomodereset = 0;
	OSD_ResizeDisplay(xres,yres);
	
#if 0
	// save the current system gamma to determine if gamma is available
	if (!gammabrightness) {
		float f = 1.0f + ((float)curbrightness / 10.0f);
        if (SDL_GetWindowGammaRamp(sdl_window, sysgamma[0], sysgamma[1], sysgamma[2]) == 0) {
            gammabrightness = 1;
		}

		// see if gamma really is working by trying to set the brightness
		if (gammabrightness && sdl_window && SDL_SetWindowBrightness(sdl_window, f) < 0) {
			gammabrightness = 0;	// nope
		}
	}
#endif
	// setpalettefade will set the palette according to whether gamma worked
	setpalettefade(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);
	
	//if (c==8) setpalette(0,256,0);
	//baselayer_onvideomodechange(c>8);
	if (regrab) grabmouse(1);

	return 0;
}


//
// resetvideomode() -- resets the video system
//
void resetvideomode(void)
{
	videomodereset = 1;
	modeschecked = 0;
}


//
// begindrawing() -- locks the framebuffer for drawing
//
void begindrawing(void)
{
	long i,j;

	if (bpp > 8) {
		if (offscreenrendering) return;
		frameplace = 0;
		bytesperline = 0;
		imageSize = 0;
		modechange = 0;
		return;
	}

	// lock the frame
	if (lockcount++ > 0)
		return;

	if (offscreenrendering) return;	
}


//
// enddrawing() -- unlocks the framebuffer
//
void enddrawing(void)
{
	if (bpp > 8) {
		if (!offscreenrendering) frameplace = 0;
		return;
	}

	if (!frameplace) return;
	if (lockcount > 1) { lockcount--; return; }
	if (!offscreenrendering) frameplace = 0;
	if (lockcount == 0) return;
	lockcount = 0;

	if (offscreenrendering) return;

}

int dnFPS = 0;

void dnCalcFPS() {
	static Uint32 prev_time = 0;
	static Uint32 frame_counter = 0;
	int current_time;
	frame_counter++;
	if (frame_counter == 50) {
		frame_counter = 0;
		current_time = SDL_GetTicks();
		if (prev_time != 0) {
			dnFPS = (int)( 50000.0 / (current_time-prev_time) );
		}
		prev_time = current_time;
	}
}

//
// showframe() -- update the display
//
void showframe(int w)
{

#ifdef USE_OPENGL
	if (bpp > 8) {
		if (palfadedelta) {
			bglMatrixMode(GL_PROJECTION);
			bglPushMatrix();
			bglLoadIdentity();
			bglMatrixMode(GL_MODELVIEW);
			bglPushMatrix();
			bglLoadIdentity();

			bglDisable(GL_DEPTH_TEST);
			bglDisable(GL_ALPHA_TEST);
			bglDisable(GL_TEXTURE_2D);

			bglEnable(GL_BLEND);
			bglColor4ub(palfadergb.r, palfadergb.g, palfadergb.b, palfadedelta);

			bglBegin(GL_QUADS);
			bglVertex2i(-1, -1);
			bglVertex2i(1, -1);
			bglVertex2i(1, 1);
			bglVertex2i(-1, 1);
			bglEnd();
			
			bglMatrixMode(GL_MODELVIEW);
			bglPopMatrix();
			bglMatrixMode(GL_PROJECTION);
			bglPopMatrix();
		}

		GUI_Render();

        SDL_GL_SwapWindow(sdl_window);
        clearview(0L);

		
		dnCalcFPS();
		if (!ud.vsync && ud.fps_max > 10) {
			Sys_ThrottleFPS(ud.fps_max);
		}

		return;
	}
#endif
	
	if (offscreenrendering) return;

	if (lockcount) {
		printf("Frame still locked %ld times when showframe() called.\n", lockcount);
		while (lockcount) enddrawing();
	}


}


//
// setpalette() -- set palette values
//
int setpalette(int start, int num, char *dapal)
{
	int i,n;

	if (bpp > 8) return 0;	// no palette in opengl

    Bmemcpy(sdlayer_pal, curpalettefaded, 256*4);
	
	for (i=start, n=num; n>0; i++, n--) {
        curpalettefaded[i].f =
        sdlayer_pal[i].a
         = 0;
		dapal += 4;
	}

    return (SDL_SetPaletteColors(sdl_palptr, sdlayer_pal, 0, 256) != 0);
}

//
// setgamma
//
int setgamma(float ro, float go, float bo)
{
    //return SDL_SetWindowBrightness(sdl_window, (ro + go + bo) / 3);
    return 1;
}

#ifndef __APPLE__
extern struct sdlappicon sdlappicon;
static SDL_Surface * loadappicon(void)
{
	SDL_Surface *surf;

	surf = SDL_CreateRGBSurfaceFrom((void*)sdlappicon.pixels,
			sdlappicon.width, sdlappicon.height, 32, sdlappicon.width*4,
			0xffl,0xff00l,0xff0000l,0xff000000l);

	return surf;
}
#endif

//
//
// ---------------------------------------
//
// Miscellany
//
// ---------------------------------------
//
// 

static
Uint32 WheelTimerCallback(Uint32 interval, void *param) {
    int32 button = (int32)param;
    mouseb &= ~(1<<button);
    return 0;
}

#ifdef _WIN32
void DSOP_Update();
#endif


static int MouseTranslate(int button)
{
    switch (button)
    {
        default:
            return -1; break;
        case SDL_BUTTON_LEFT:
            return 0; break;
        case SDL_BUTTON_RIGHT:
            return 1; break;
        case SDL_BUTTON_MIDDLE:
            return 2; break;

        // On SDL2/Windows, everything is as it should be.
        case SDL_BUTTON_X1:
            return 5; break;

        case SDL_BUTTON_X2:
            return 6; break;
    }

    return -1;
}

static
float applyDeadZone(float value, float deadZoneSize) {
    if (value < -deadZoneSize) {
        value -= deadZoneSize;
    } else if (value > deadZoneSize) {
        value += deadZoneSize;
    } else {
        return 0.0f;
    }
    
    return value < -1.0 ? -1.0 : (value > 1.0 ? 1.0 : value);
}

static Uint8 g_keyBuffer0[DN_MAX_KEYS] = { 0 };
static Uint8 g_keyBuffer1[DN_MAX_KEYS] = { 0 };
static Uint8 *g_keyBufferPtrCurrent = &g_keyBuffer0[0];
static Uint8 *g_keyBufferPtrPrevious = &g_keyBuffer1[0];

static
void updateKeyBuffers( SDL_Event *ev ) {
	Uint8 *tmp;
	int numkeys;
	const Uint8 *keystate;
	Uint32 buttons;
	int i = 0;
	
	tmp = g_keyBufferPtrCurrent;
	g_keyBufferPtrCurrent = g_keyBufferPtrPrevious;
	g_keyBufferPtrPrevious = tmp;
	
	keystate = SDL_GetKeyboardState( &numkeys );
	buttons = SDL_GetMouseState( NULL, NULL );
	
	memcpy( g_keyBufferPtrCurrent, keystate, numkeys );
	memset( g_keyBufferPtrCurrent + numkeys, 0, DN_MAX_KEYS - numkeys );
	
	g_keyBufferPtrCurrent[DNK_MENU_NEXT_OPTION] = g_keyBufferPtrCurrent[SDL_SCANCODE_RIGHT] || GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
	g_keyBufferPtrCurrent[DNK_MENU_PREV_OPTION] = g_keyBufferPtrCurrent[SDL_SCANCODE_LEFT]  ||  GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
	g_keyBufferPtrCurrent[DNK_MENU_UP] = g_keyBufferPtrCurrent[SDL_SCANCODE_UP] || GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_UP);
	g_keyBufferPtrCurrent[DNK_MENU_DOWN] = g_keyBufferPtrCurrent[SDL_SCANCODE_DOWN] || GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	g_keyBufferPtrCurrent[DNK_MENU_ENTER] = g_keyBufferPtrCurrent[SDL_SCANCODE_RETURN] || g_keyBufferPtrCurrent[SDL_SCANCODE_KP_ENTER] || GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_A);
	g_keyBufferPtrCurrent[DNK_MENU_CLEAR] = g_keyBufferPtrCurrent[SDL_SCANCODE_BACKSPACE] || g_keyBufferPtrCurrent[SDL_SCANCODE_DELETE] || GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_Y);
	g_keyBufferPtrCurrent[DNK_MENU_REFRESH] = g_keyBufferPtrCurrent[SDL_SCANCODE_SPACE] || GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_X);
    g_keyBufferPtrCurrent[DNK_MENU_RESET] = g_keyBufferPtrCurrent[SDL_SCANCODE_F8] || GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_BACK);
	
	for (i = 0; i < (DNK_MOUSE_LAST - DNK_MOUSE_FIRST); i++ ) {
        g_keyBufferPtrCurrent[DNK_MOUSE0 + i] = SDL_BUTTON( i + 1 ) & buttons ? 1 : 0;
    }
    
	if ( ev->type == SDL_MOUSEWHEEL ) {
		if ( ev->wheel.y > 0 ) {
			g_keyBufferPtrCurrent[DNK_MOUSE_WHEELUP] = 1;
            g_keyBufferPtrPrevious[DNK_MOUSE_WHEELUP] = 0;
		} else if ( ev->wheel.y < 0 ) {
			g_keyBufferPtrCurrent[DNK_MOUSE_WHEELDOWN] = 1;
            g_keyBufferPtrPrevious[DNK_MOUSE_WHEELDOWN] = 0;
		}
	}
    
    for (i = 0; i < DNK_GAMEPAD_LAST - DNK_GAMEPAD_FIRST - 1; i++ ) {
        if (i == DNK_GAMEPAD_START || i == DNK_GAMEPAD_GUIDE) {
            continue;
        } else {
            g_keyBufferPtrCurrent[DNK_GAMEPAD_FIRST + i] = GP_GET_BUTTON(SDL_CONTROLLER_BUTTON_A + i);
        }
	}
    
    g_keyBufferPtrCurrent[DNK_GAMEPAD_LT] = (applyDeadZone((float)GP_GET_AXIS(SDL_CONTROLLER_AXIS_TRIGGERLEFT)/GP_AXIS_MAX_VALUE, GP_AXIS_DEADZONE) == 1.0f);
    g_keyBufferPtrCurrent[DNK_GAMEPAD_RT] = (applyDeadZone((float)GP_GET_AXIS(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)/GP_AXIS_MAX_VALUE, GP_AXIS_DEADZONE) == 1.0f);
}

#define SetKey(key,state) { \
keystatus[key] = state; \
if (state) { \
keyfifo[keyfifoend] = key; \
keyfifo[(keyfifoend+1)&(KEYFIFOSIZ-1)] = state; \
keyfifoend = ((keyfifoend+2)&(KEYFIFOSIZ-1)); \
} \
}

static
void processKeyDown( dnKey scancode ) {
	int code;
	
	if ( scancode == SDL_SCANCODE_ESCAPE || scancode == DNK_GAMEPAD_START ) {
		keystatus[1] = 1;
	} else {
		dnPressKey( scancode );
	}
	
	if (
		((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc &&
		(scancode == SDL_SCANCODE_RETURN ||
		 scancode == SDL_SCANCODE_KP_ENTER ||
		 scancode == SDL_SCANCODE_ESCAPE ||
		 scancode == SDL_SCANCODE_BACKSPACE ||
		 scancode == SDL_SCANCODE_TAB))
	{
		char keyvalue;
		switch (scancode)
		{
			case SDL_SCANCODE_RETURN:
			case SDL_SCANCODE_KP_ENTER: keyvalue = '\r'; break;
			case SDL_SCANCODE_ESCAPE: keyvalue = 27; break;
			case SDL_SCANCODE_BACKSPACE: keyvalue = '\b'; break;
			case SDL_SCANCODE_TAB: keyvalue = '\t'; break;
			default: keyvalue = 0; break;
		}
		keyasciififo[keyasciififoend] = keyvalue;
		keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
	}
	
	code = keytranslation[scancode];
	if (scancode != SDL_SCANCODE_ESCAPE && code != 1)
	SetKey(code, 1);
	if (keypresscallback) {
		keypresscallback(code, 1);
	}
}

static
void processKeyUp( dnKey scancode ) {
	int code;

	if ( scancode == SDL_SCANCODE_ESCAPE || scancode == DNK_GAMEPAD_START ) {
		keystatus[1] = 0;
	} else {
		dnReleaseKey( scancode );
	}
	code = keytranslation[scancode];
	SetKey( code, 0 );
	if ( keypresscallback ) {
		keypresscallback( code, 0 );
	}
}

int dnKeyJustPressed( dnKey key ) {
	return g_keyBufferPtrPrevious[key] == 0 && g_keyBufferPtrCurrent[key] != 0;
}

int dnKeyJustReleased( dnKey key ) {
	return g_keyBufferPtrPrevious[key] != 0 && g_keyBufferPtrCurrent[key] == 0;
}

static
void processKeyBuffers( void ) {
	int i = 0;
	for (i = 0; i < DN_MAX_KEYS; i++ ) {
		if ( dnKeyJustPressed( i ) ) {
			processKeyDown( (dnKey)i );
		}
		if ( dnKeyJustReleased( i ) ) {
			processKeyUp( (dnKey)i );
		}
	}
}


//
// handleevents() -- process the SDL message queue
//   returns !0 if there was an important event worth checking (like quitting)
//
int handleevents(void)
{
	int code, rv=0, j;
	SDL_Event ev;
	static int firstcall = 1;

    dnUpdateMouseInput();
	while (SDL_PollEvent(&ev)) {
		updateKeyBuffers( &ev );
		if (GUI_InjectEvent(&ev)) {
			/* GUI ate the event, don't let it pass through */
			continue;
		}
		processKeyBuffers();

		switch (ev.type) {
            case SDL_TEXTINPUT:
                j = 0;
                do
                {
                    code = ev.text.text[j];

                    if (((keyasciififoend+1)&(KEYFIFOSIZ-1)) != keyasciififoplc)
                    {
                        keyasciififo[keyasciififoend] = code;
                        keyasciififoend = ((keyasciififoend+1)&(KEYFIFOSIZ-1));
                    }
                }
                while (j < SDL_TEXTINPUTEVENT_TEXT_SIZE && ev.text.text[++j]);
                break;
                
            case SDL_JOYDEVICEADDED:
            case SDL_JOYDEVICEREMOVED:
                getgamepad();
                break;
                
            case SDL_KEYUP:
                break;
            case SDL_CONTROLLERBUTTONUP:
                gamepadButtonPressed = 0;
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                gamepadButtonPressed = 1;
                break;
            case SDL_WINDOWEVENT:
                switch (ev.window.event)
                {
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    appactive = 1;
                    grabmouse(1);
                    break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    appactive = 0;
                    grabmouse(0);
                    break;
                }
                break;

			case SDL_MOUSEMOTION:
                if (!firstcall) {
                    if ( appactive ) {
                        mousex += ev.motion.xrel;
                        mousey += ev.motion.yrel;
                    }
                }
				break;				
			case SDL_QUIT:
				quitevent = 1;
				rv=-1;
				break;

			default:
				//printOSD("Got event (%d)\n", ev.type);
				break;
		}
	}
    
	GUI_TimePulse();
#ifdef _WIN32
	DSOP_Update();
#endif
	sampletimer();
	startwin_idle(NULL);
#undef SetKey

	firstcall = 0;
	
	return rv;
}



static int32_t buildkeytranslationtable(void)
{
    memset(keytranslation,0,sizeof(keytranslation));

#define MAP(x,y) keytranslation[x] = y
    MAP(SDL_SCANCODE_BACKSPACE,	0xe);
    MAP(SDL_SCANCODE_TAB,		0xf);
    MAP(SDL_SCANCODE_RETURN,	0x1c);
    MAP(SDL_SCANCODE_PAUSE,		0x59);	// 0x1d + 0x45 + 0x9d + 0xc5
    MAP(SDL_SCANCODE_ESCAPE,	0x1);
    MAP(SDL_SCANCODE_SPACE,		0x39);
    MAP(SDL_SCANCODE_COMMA,		0x33);
    MAP(SDL_SCANCODE_MINUS,		0xc);
    MAP(SDL_SCANCODE_PERIOD,	0x34);
    MAP(SDL_SCANCODE_SLASH,		0x35);
    MAP(SDL_SCANCODE_0,		0xb);
    MAP(SDL_SCANCODE_1,		0x2);
    MAP(SDL_SCANCODE_2,		0x3);
    MAP(SDL_SCANCODE_3,		0x4);
    MAP(SDL_SCANCODE_4,		0x5);
    MAP(SDL_SCANCODE_5,		0x6);
    MAP(SDL_SCANCODE_6,		0x7);
    MAP(SDL_SCANCODE_7,		0x8);
    MAP(SDL_SCANCODE_8,		0x9);
    MAP(SDL_SCANCODE_9,		0xa);
    MAP(SDL_SCANCODE_SEMICOLON,	0x27);
    MAP(SDL_SCANCODE_APOSTROPHE, 0x28);
    MAP(SDL_SCANCODE_EQUALS,	0xd);
    MAP(SDL_SCANCODE_LEFTBRACKET,	0x1a);
    MAP(SDL_SCANCODE_BACKSLASH,	0x2b);
    MAP(SDL_SCANCODE_RIGHTBRACKET,	0x1b);
    MAP(SDL_SCANCODE_A,		0x1e);
    MAP(SDL_SCANCODE_B,		0x30);
    MAP(SDL_SCANCODE_C,		0x2e);
    MAP(SDL_SCANCODE_D,		0x20);
    MAP(SDL_SCANCODE_E,		0x12);
    MAP(SDL_SCANCODE_F,		0x21);
    MAP(SDL_SCANCODE_G,		0x22);
    MAP(SDL_SCANCODE_H,		0x23);
    MAP(SDL_SCANCODE_I,		0x17);
    MAP(SDL_SCANCODE_J,		0x24);
    MAP(SDL_SCANCODE_K,		0x25);
    MAP(SDL_SCANCODE_L,		0x26);
    MAP(SDL_SCANCODE_M,		0x32);
    MAP(SDL_SCANCODE_N,		0x31);
    MAP(SDL_SCANCODE_O,		0x18);
    MAP(SDL_SCANCODE_P,		0x19);
    MAP(SDL_SCANCODE_Q,		0x10);
    MAP(SDL_SCANCODE_R,		0x13);
    MAP(SDL_SCANCODE_S,		0x1f);
    MAP(SDL_SCANCODE_T,		0x14);
    MAP(SDL_SCANCODE_U,		0x16);
    MAP(SDL_SCANCODE_V,		0x2f);
    MAP(SDL_SCANCODE_W,		0x11);
    MAP(SDL_SCANCODE_X,		0x2d);
    MAP(SDL_SCANCODE_Y,		0x15);
    MAP(SDL_SCANCODE_Z,		0x2c);
    MAP(SDL_SCANCODE_DELETE,	0xd3);
    MAP(SDL_SCANCODE_KP_0,		0x52);
    MAP(SDL_SCANCODE_KP_1,		0x4f);
    MAP(SDL_SCANCODE_KP_2,		0x50);
    MAP(SDL_SCANCODE_KP_3,		0x51);
    MAP(SDL_SCANCODE_KP_4,		0x4b);
    MAP(SDL_SCANCODE_KP_5,		0x4c);
    MAP(SDL_SCANCODE_KP_6,		0x4d);
    MAP(SDL_SCANCODE_KP_7,		0x47);
    MAP(SDL_SCANCODE_KP_8,		0x48);
    MAP(SDL_SCANCODE_KP_9,		0x49);
    MAP(SDL_SCANCODE_KP_PERIOD,	0x53);
    MAP(SDL_SCANCODE_KP_DIVIDE,	0xb5);
    MAP(SDL_SCANCODE_KP_MULTIPLY,	0x37);
    MAP(SDL_SCANCODE_KP_MINUS,	0x4a);
    MAP(SDL_SCANCODE_KP_PLUS,	0x4e);
    MAP(SDL_SCANCODE_KP_ENTER,	0x9c);
    //MAP(SDL_SCANCODE_KP_EQUALS,	);
    MAP(SDL_SCANCODE_UP,		0xc8);
    MAP(SDL_SCANCODE_DOWN,		0xd0);
    MAP(SDL_SCANCODE_RIGHT,		0xcd);
    MAP(SDL_SCANCODE_LEFT,		0xcb);
    MAP(SDL_SCANCODE_INSERT,	0xd2);
    MAP(SDL_SCANCODE_HOME,		0xc7);
    MAP(SDL_SCANCODE_END,		0xcf);
    MAP(SDL_SCANCODE_PAGEUP,	0xc9);
    MAP(SDL_SCANCODE_PAGEDOWN,	0xd1);
    MAP(SDL_SCANCODE_F1,		0x3b);
    MAP(SDL_SCANCODE_F2,		0x3c);
    MAP(SDL_SCANCODE_F3,		0x3d);
    MAP(SDL_SCANCODE_F4,		0x3e);
    MAP(SDL_SCANCODE_F5,		0x3f);
    MAP(SDL_SCANCODE_F6,		0x40);
    MAP(SDL_SCANCODE_F7,		0x41);
    MAP(SDL_SCANCODE_F8,		0x42);
    MAP(SDL_SCANCODE_F9,		0x43);
    MAP(SDL_SCANCODE_F10,		0x44);
    MAP(SDL_SCANCODE_F11,		0x57);
    MAP(SDL_SCANCODE_F12,		0x58);
    MAP(SDL_SCANCODE_NUMLOCKCLEAR,	0x45);
    MAP(SDL_SCANCODE_CAPSLOCK,	0x3a);
    MAP(SDL_SCANCODE_SCROLLLOCK,	0x46);
    MAP(SDL_SCANCODE_RSHIFT,	0x36);
    MAP(SDL_SCANCODE_LSHIFT,	0x2a);
    MAP(SDL_SCANCODE_RCTRL,		0x9d);
    MAP(SDL_SCANCODE_LCTRL,		0x1d);
    MAP(SDL_SCANCODE_RALT,		0xb8);
    MAP(SDL_SCANCODE_LALT,		0x38);
    MAP(SDL_SCANCODE_LGUI,	0xdb);	// win l
    MAP(SDL_SCANCODE_RGUI,	0xdc);	// win r
    MAP(SDL_SCANCODE_PRINTSCREEN,		-2);	// 0xaa + 0xb7
    MAP(SDL_SCANCODE_SYSREQ,	0x54);	// alt+printscr
//    MAP(SDL_SCANCODE_PAUSE,		0xb7);	// ctrl+pause
    MAP(SDL_SCANCODE_MENU,		0xdd);	// win menu?
    MAP(SDL_SCANCODE_GRAVE,     0x29);  // tilde
#undef MAP

    return 0;
}

#if defined _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#elif defined __linux || defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__
# include <sys/mman.h>
#endif

void makeasmwriteable(void)
{
#ifndef ENGINE_USING_A_C
	extern int dep_begin, dep_end;
# if defined _WIN32
	DWORD oldprot;
	if (!VirtualProtect((LPVOID)&dep_begin, (SIZE_T)&dep_end - (SIZE_T)&dep_begin, PAGE_EXECUTE_READWRITE, &oldprot)) {
		initprint("Error making code writeable\n");
		return;
	}
# elif defined __linux || defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__
	int pagesize;
	size_t dep_begin_page;
	pagesize = sysconf(_SC_PAGE_SIZE);
	if (pagesize == -1) {
		initprintf("Error getting system page size\n");
		return;
	}
	dep_begin_page = ((size_t)&dep_begin) & ~(pagesize-1);
	if (mprotect((const void *)dep_begin_page, (size_t)&dep_end - dep_begin_page, PROT_READ|PROT_WRITE) < 0) {
		initprintf("Error making code writeable (errno=%d)\n", errno);
		return;
	}
# else
#  error Dont know how to unprotect the self-modifying assembly on this platform!
# endif
#endif
}

/* dnAPI */

void dnGetCurrentVideoMode(VideoMode *videomode) {
	SDL_DisplayMode displayMode;
	assert(videomode != NULL);

	SDL_GetWindowDisplayMode( sdl_window, &displayMode );
	videomode->fullscreen = (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_FULLSCREEN) ? 1 : 0;
	videomode->width = displayMode.w;
	videomode->height = displayMode.h;
	videomode->bpp = 32;
}

static int clampi(int v, int min, int max) {
    if (v > max) return max;
    if (v < min) return min;
    return v;
}

static double clampd(double v, double min, double max) {
    if (v > max) return max;
    if (v < min) return min;
    return v;
}

#define TURBOTURNTIME (TICRATE/8) // 7

void dnOverrideInputGamepadMove(long *vel, long *svel) {
    SDL_GameControllerAxis leftStickX = SDL_CONTROLLER_AXIS_LEFTX;
    SDL_GameControllerAxis leftStickY = SDL_CONTROLLER_AXIS_LEFTY;
    long move_y = 0, move_x = 0;
    if (!dnGamepadConnected())
        return;
    if (movestickleft != 1) {
        leftStickX = SDL_CONTROLLER_AXIS_RIGHTX;
        leftStickY = SDL_CONTROLLER_AXIS_RIGHTY;
    }
    move_y = (long)clamp((long)(-80*applyDeadZone((float)GP_GET_AXIS(leftStickY)/GP_AXIS_MAX_VALUE, GP_AXIS_DEADZONE)), -80, 80);
    move_x = (long)clamp((long)(-90*applyDeadZone((float)GP_GET_AXIS(leftStickX)/GP_AXIS_MAX_VALUE, GP_AXIS_DEADZONE)), -90, 90);
    if (move_y != 0) *vel = move_y;
    if (move_x != 0) *svel = move_x;
}

typedef struct {
    double x, y;
} vector2;

vector2 sensitivity = { 10.0, 13.0 };
vector2 max_velocity = { 1000.0, 1000.0 };

void dnOverrideInput(input *loc) {
    
    extern long totalclock;
    static int32 heldlooktime = 0;
    static int32 tics = 0;
    static int32 lastcontroltime = 0;

    vector2 total_velocity = { 0.0, 0.0 };
    vector2 mouse_velocity = { 0.0, 0.0 };
    vector2 gamepad_velocity = { 0.0, 0.0 };
    vector2 pointer = { 0.0, 0.0 };
    int imousex, imousey;
    long lmousex, lmousey;
    
    float xsaturation, ysaturation;
    SDL_GameControllerAxis rightStickX = SDL_CONTROLLER_AXIS_RIGHTX;
    SDL_GameControllerAxis rightStickY = SDL_CONTROLLER_AXIS_RIGHTY;
    if (movestickleft != 1) {
        rightStickX = SDL_CONTROLLER_AXIS_LEFTX;
        rightStickY = SDL_CONTROLLER_AXIS_LEFTY;
    }

    if ( dnGetNumberOfMouseInputDevices() > 0 ) { 
        dnGetMouseInput( &imousex, &imousey, 0 );
        pointer.x = imousex;
        pointer.y = imousey;
    } else {
        readmousexy( &lmousex, &lmousey );
        pointer.x = lmousex;
        pointer.y = lmousey;
    }
        
    if (dnGamepadConnected()) {
        xsaturation = applyDeadZone((float)GP_GET_AXIS(rightStickX)/GP_AXIS_MAX_VALUE, GP_AXIS_DEADZONE);
        gamepad_velocity.x = 127 * (xgamepadscale/10.0) * xsaturation;
    }
    mouse_velocity.x = pointer.x*sensitivity.x*(xmousescale/10.0);
    if (ps[myconnectindex].aim_mode) {
        mouse_velocity.y = pointer.y*sensitivity.y*(((double)xdim)/((double)ydim))*(ymousescale/10.0);
        if (dnGamepadConnected()) {
            ysaturation = applyDeadZone((float)GP_GET_AXIS(rightStickY)/GP_AXIS_MAX_VALUE, GP_AXIS_DEADZONE);
            gamepad_velocity.y = 127 * (ygamepadscale/10.0) * ysaturation;
        }
    } else {
        loc->horz = 1;
    }

    if (dnGamepadConnected()) {
        if (gamepad_velocity.x != 0 && ((xsaturation >= 1.0f) || (xsaturation <= -1.0f))) {
            heldlooktime += tics;
            if (heldlooktime > (TURBOTURNTIME * 2)) {
                gamepad_velocity.x += gamepad_velocity.x/2;
            }
        } else if (gamepad_velocity.y != 0 && ((ysaturation >= 1.0f) || (ysaturation <= -1.0f))) {
            heldlooktime += tics;
            if (heldlooktime > (TURBOTURNTIME * 2)) {
                gamepad_velocity.y += gamepad_velocity.y/2;
            }
        } else {
            heldlooktime = 0;
        }
        total_velocity.x += gamepad_velocity.x;
        total_velocity.y += gamepad_velocity.y;
    }
    
    total_velocity.x += mouse_velocity.x;
    total_velocity.y += mouse_velocity.y;

    total_velocity.x = clampd(total_velocity.x, -max_velocity.x, max_velocity.x);
    total_velocity.y = clampd(total_velocity.y, -max_velocity.y, max_velocity.y);

    loc->avel = (signed char) clampi((int)(128*total_velocity.x/max_velocity.x) + loc->avel, -127, 127);
    if (loc->avel != 0) {
        pointer.x = 0.0;
    }
    
    loc->horz = (signed char) clampi((int)(128*total_velocity.y/max_velocity.y) + loc->horz, -127, 127);
    if (loc->horz != 0) {
        pointer.y = 0.0;
    }
    if (!ud.mouseflip) {
        loc->horz *= -1;
    }
    tics = totalclock-lastcontroltime;
    lastcontroltime = totalclock;
}

void dnSetMouseSensitivity(int sens) {
    sensitivity.x = 1.0 + sens/65536.0*16.0;
    sensitivity.y = sensitivity.x*1.2;
}

int dnGetMouseSensitivity() {
    return (int)((sensitivity.x-1.0)*65536.0/16.0);
}
