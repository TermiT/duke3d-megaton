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

/*
int dnTranslateSDLKey(SDLKey key);
const char* dnGetKeyName(int key);
void dnAssignKey(int function, int whichkey, int keycode);
void dnResetMouseKeyBindings(void);
*/
    
void dnGetCloudFilesName(char *names[]);
void dnPullCloudFiles();
void dnPushCloudFiles();
void dnOverrideInput(input *loc);
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

typedef SDLKey dnKey;

#define DN_MAX_KEYS 512
    
#define DNK_FIRST (SDLK_LAST+1)

#define DNK_MOUSE0 (DNK_FIRST+0)
#define DNK_MOUSE1 (DNK_FIRST+1)
#define DNK_MOUSE2 (DNK_FIRST+2)
#define DNK_MOUSE3 (DNK_FIRST+3)
#define DNK_MOUSE4 (DNK_FIRST+4)
#define DNK_MOUSE5 (DNK_FIRST+5)
#define DNK_MOUSE6 (DNK_FIRST+6)
#define DNK_MOUSE7 (DNK_FIRST+7)
#define DNK_MOUSE8 (DNK_FIRST+8)
#define DNK_MOUSE9 (DNK_FIRST+9)
#define DNK_MOUSE10 (DNK_FIRST+10)
#define DNK_MOUSE11 (DNK_FIRST+11)
#define DNK_MOUSE12 (DNK_FIRST+12)
#define DNK_MOUSE13 (DNK_FIRST+13)
#define DNK_MOUSE14 (DNK_FIRST+14)
#define DNK_MOUSE15 (DNK_FIRST+15)
#define DNK_MOUSE_LAST DNK_MOUSE15

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
    
void dnResetBindings();
void dnBindFunction(int function, int slot, dnKey key);
SDLKey dnGetFunctionBinding(int function, int slot);
void dnClearFunctionBindings();
void dnApplyBindings();
void dnDetectVideoMode();
char * dnFilterUsername(const char * name);

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
