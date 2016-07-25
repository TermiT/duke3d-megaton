//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

// some parts taken from DN3D for xbox

#ifndef DNAPI_H
#define DNAPI_H

#include "SDL.h"
#ifdef __cplusplus
#undef max 
#undef min 

#include <vector>
extern "C" {
#endif
    
#include "duke3d.h"
#include "csteam.h"
    
#pragma pack(push, 1)

    
typedef struct _GameDesc {
	unsigned char level;
	unsigned char volume;
	unsigned char player_skill;
	unsigned char monsters_off;
	unsigned char respawn_monsters;
	unsigned char respawn_items;
	unsigned char respawn_inventory;
    unsigned char netgame;
    unsigned char numplayers;
	unsigned char coop;
	unsigned char marker;
	unsigned char ffire;
	int32 fraglimit;
	int32 timelimit;
	unsigned char SplitScreen;
    steam_id_t    own_id;
    steam_id_t    other_ids[MAXPLAYERS];
} GameDesc;

typedef struct _VideMode {
	int width;
	int height;
	int bpp;
	int fullscreen;
} VideoMode;
#pragma pack(pop)

void Sys_ThrottleFPS(int max_fps);

void dnNewGame(GameDesc *gamedesc);
void dnQuitGame();
void dnHideMenu();
int  dnGameModeSet();
int  dnMenuModeSet();
void dnGetDefaultMode(VideoMode *vm);

void dnGetCurrentVideoMode(VideoMode *videomode);
void dnChangeVideoMode(VideoMode *videmode);
void dnSetBrightness(int brightness); /* 0..63 */
int dnGetBrightness();

void dnEnableSound(int enable);
void dnEnableMusic(int enable);
void dnEnableVoice(int enable);
void dnSetSoundVolume(int volume);
void dnSetMusicVolume(int volume);
void dnSetStatusbarMode(int mode);

void dnGetCloudFilesName(char *names[]);
void dnPullCloudFiles();
void dnPushCloudFiles();
void dnOverrideInput(input *loc);
void dnOverrideInputGamepadMove(long *vel, long *svel);
void dnSetMouseSensitivity(int sens);
int dnGetMouseSensitivity();
int dnGetTile(int tile_no, int *width, int *height, void *data);
int dnLoadGame(int slot);
void dnCaptureScreen();
int dnSaveGame(int slot);
void dnQuitToTitle(const char *message);
void dnGetQuitMessage(char *message);
void dnResetMouse();
void dnResetMouseWheel();
int dnIsVsyncOn();


int dnGetAddonId();
int dnGetAddonEpisode();
const char* dnGetGRPName();
const char* dnGetVersion();

void dnSetLastSaveSlot(short i);
CACHE1D_FIND_REC *dnGetMapsList();
void dnSetUserMap(const char * mapname);
void dnSetWorkshopMap(const char * mapname, const char * zipname);
void dnUnsetWorkshopMap();
int dnIsUserMap();
const char* dnGetEpisodeName(int episode);
const char* dnGetLevelName(int episode, int level);

/* New control system */

typedef SDL_Scancode dnKey;

enum {
    SDL_MAX_KEYS = SDL_NUM_SCANCODES,
    DN_MAX_KEYS = ( SDL_MAX_KEYS + 64 ),
	
    DNK_FIRST = ( SDL_MAX_KEYS ),
	
	DNK_MOUSE_FIRST = ( DNK_FIRST ),
    DNK_MOUSE0 =      ( DNK_MOUSE_FIRST + 0 ),
    DNK_MOUSE1 =      ( DNK_MOUSE_FIRST + 1 ),
    DNK_MOUSE2 =      ( DNK_MOUSE_FIRST + 2 ),
    DNK_MOUSE3 =      ( DNK_MOUSE_FIRST + 3 ),
    DNK_MOUSE4 =      ( DNK_MOUSE_FIRST + 4 ),
    DNK_MOUSE5 =      ( DNK_MOUSE_FIRST + 5 ),
    DNK_MOUSE6 =      ( DNK_MOUSE_FIRST + 6 ),
    DNK_MOUSE7 =      ( DNK_MOUSE_FIRST + 7 ),
    DNK_MOUSE8 =      ( DNK_MOUSE_FIRST + 8 ),
    DNK_MOUSE9 =      ( DNK_MOUSE_FIRST + 9 ),
    DNK_MOUSE10 =     ( DNK_MOUSE_FIRST + 10 ),
    DNK_MOUSE11 =     ( DNK_MOUSE_FIRST + 11 ),
    DNK_MOUSE12 =     ( DNK_MOUSE_FIRST + 12 ),
    DNK_MOUSE13 =     ( DNK_MOUSE_FIRST + 13 ),
    DNK_MOUSE14 =     ( DNK_MOUSE_FIRST + 14 ),
    DNK_MOUSE15 =     ( DNK_MOUSE_FIRST + 15 ),
    DNK_MOUSE_LAST =  ( DNK_MOUSE15 ),
    
    DNK_MOUSE_WHEELUP = (DNK_MOUSE_LAST + 1),
    DNK_MOUSE_WHEELDOWN = (DNK_MOUSE_LAST + 2),
	
	DNK_GAMEPAD_FIRST =  ( DNK_MOUSE_WHEELDOWN + 1),
    DNK_GAMEPAD_A =      ( DNK_GAMEPAD_FIRST + 0 ),
	DNK_GAMEPAD_B =      ( DNK_GAMEPAD_FIRST + 1 ),
	DNK_GAMEPAD_X =      ( DNK_GAMEPAD_FIRST + 2 ),
	DNK_GAMEPAD_Y =      ( DNK_GAMEPAD_FIRST + 3 ),
    DNK_GAMEPAD_BACK =   ( DNK_GAMEPAD_FIRST + 4 ),
    DNK_GAMEPAD_GUIDE =  ( DNK_GAMEPAD_FIRST + 5 ),
    DNK_GAMEPAD_START =  ( DNK_GAMEPAD_FIRST + 6 ),
    DNK_GAMEPAD_LSTICK = ( DNK_GAMEPAD_FIRST + 7 ),
    DNK_GAMEPAD_RSTICK = ( DNK_GAMEPAD_FIRST + 8 ),
	DNK_GAMEPAD_LB =     ( DNK_GAMEPAD_FIRST + 9 ),
	DNK_GAMEPAD_RB =     ( DNK_GAMEPAD_FIRST + 10 ),
    DNK_GAMEPAD_UP =     ( DNK_GAMEPAD_FIRST + 11 ),
	DNK_GAMEPAD_DOWN =   ( DNK_GAMEPAD_FIRST + 12 ),
	DNK_GAMEPAD_LEFT =   ( DNK_GAMEPAD_FIRST + 13 ),
	DNK_GAMEPAD_RIGHT =  ( DNK_GAMEPAD_FIRST + 14 ),
	DNK_GAMEPAD_LT =     ( DNK_GAMEPAD_FIRST + 15 ),
	DNK_GAMEPAD_RT =     ( DNK_GAMEPAD_FIRST + 16 ),
	DNK_GAMEPAD_LAST =   ( DNK_GAMEPAD_RT),
	
	DNK_MENU_FIRST =       ( DNK_GAMEPAD_LAST + 1),
	DNK_MENU_NEXT_OPTION = ( DNK_MENU_FIRST + 0 ),
	DNK_MENU_PREV_OPTION = ( DNK_MENU_FIRST + 1 ),
	DNK_MENU_UP =          ( DNK_MENU_FIRST + 2 ),
	DNK_MENU_DOWN =        ( DNK_MENU_FIRST + 3 ),
	DNK_MENU_ENTER =       ( DNK_MENU_FIRST + 4 ),
	DNK_MENU_CLEAR =       ( DNK_MENU_FIRST + 5 ),
	DNK_MENU_REFRESH =     ( DNK_MENU_FIRST + 6 ),
	DNK_MENU_BACK =        ( DNK_MENU_FIRST + 7 ),
    DNK_MENU_RESET =        ( DNK_MENU_FIRST + 8 ),
	DNK_MENU_LAST =        ( DNK_MENU_BACK ),
};

void dnInitKeyNames();
void dnAssignKey(dnKey key, int action);
int  dnGetKeyAction(dnKey key);
const char* dnGetKeyName(dnKey key);
dnKey dnGetKeyByName(const char *keyName);
void dnPressKey(dnKey key);
void dnReleaseKey(dnKey key);
void dnClearKeys();
int  dnKeyState(dnKey key);
void dnGetInput();
int dnKeyJustPressed( dnKey key );
int dnKeyJustReleased( dnKey key );

    
void dnResetBindings();
void dnBindFunction(int function, int slot, dnKey key);
dnKey dnGetFunctionBinding(int function, int slot);
void dnClearFunctionBindings();
void dnApplyBindings();
void dnDetectVideoMode();
char * dnFilterUsername(const char * name);
    
void dnGrabMouse( int grab );
int dnGamepadConnected();
void dnVibrateGamepad (float force, int time);
int dnCanVibrate();

#define DN_VIDEO_STRETCH 0
#define DN_VIDEO_16_9 1
#define DN_VIDEO_16_10 2
#define DN_VIDEO_4_3 3

void dnInitVideoPlayback( int aspect ); /* 0 - stretch; 1 - 16:9; 2 - 16:10; 3 - 4:3 */
void dnDrawQuad( int gltex );
void dnSwapBuffers( void );

void Sys_GetScreenSize(int *width, int *height);
void Sys_CenterWindow(int width, int height);
double Sys_GetTicks();
void Sys_DPrintf(const char *format, ...);
int Sys_FileExists(const char *path);
    
void play_vpx_video(const char * filename, void (*frame_callback)(int));
    
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

typedef std::vector<VideoMode> VideoModeList;

void dnGetVideoModeList(VideoModeList& modes);

bool operator == (const VideoMode& a, const VideoMode& b);

inline
bool operator != (const VideoMode& a, const VideoMode& b) {
	return !(a==b);
}

#endif

#endif /* DNAPI_H */
