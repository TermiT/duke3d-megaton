//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include <map>
#include <set>
#include <algorithm>
#include <stdio.h>
#include "playvpx/playvpx.h"
#include "dnAPI.h"
#include "SDL.h"
#include "build.h"
#include "helpers.h"
#include "csteam.h"
#ifdef  __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

void dnGetVideoModeList(VideoModeList& modes) {
	typedef std::pair<int, int> ScreenSize;
	typedef std::pair<ScreenSize, int> ScreenMode;
	typedef std::set<ScreenMode> ModeSet;
	ModeSet mode_set;

	for (int i = 0; i < validmodecnt; i++) {
		int bpp_fs = 32;
        {
            if (validmode[i].xdim >= 2880 && validmode[i].ydim >= 1800)
                continue;
			ScreenSize size(validmode[i].xdim, validmode[i].ydim);
			ScreenMode mode(size, bpp_fs);
			mode_set.insert(mode);
			if (mode.second == 32) {
				mode.second = 24;
				if (mode_set.find(mode) != mode_set.end()) {
					mode_set.erase(mode);	
				}
			}
		}
	}
	modes.resize(0);
	modes.reserve(mode_set.size());
	for (ModeSet::iterator i = mode_set.begin(); i != mode_set.end(); i++) {
		VideoMode vm;
		ScreenSize size = i->first;
		vm.width = size.first;
		vm.height = size.second;
		vm.bpp = i->second;
		vm.fullscreen = 0;
		modes.push_back(vm);
	}
}

extern "C" long setgamemode(char davidoption, long daxdim, long daydim, long dabpp, int force);
extern "C" void polymost_glreset ();

void dnChangeVideoMode(VideoMode *v) {
	//setvideomode(videomode->width, videomode->height, videomode->bpp, videomode->fullscreen);
	//resetvideomode();
	polymost_glreset();
	setgamemode(v->fullscreen, v->width, v->height, v->bpp, 1);
}

bool operator == (const VideoMode& a, const VideoMode& b) {
	return (a.bpp==b.bpp) && (clamp(a.fullscreen,0,1)==clamp(b.fullscreen,0,1)) && (a.height==b.height) && (a.width==b.width);
}

extern "C" {
    extern int delete_saves;
}

void dnPullCloudFiles() {
    extern char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH];
	for (int i = 0; i < MAX_CLOUD_FILES; i++) {
		CSTEAM_DownloadFile(cloudFileNames[i]);
        if (delete_saves && i != MAX_CLOUD_FILES) { //delete only saves, don't delete duke3d.cfg
            CSTEAM_DeleteCloudFile(cloudFileNames[i]);
            unlink(cloudFileNames[i]);
        }

	}
}

void dnPushCloudFiles() {
    extern char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH];
	for (int i = 0; i < MAX_CLOUD_FILES; i++) {
		CSTEAM_UploadFile(cloudFileNames[i]);
	}
}

extern "C" SDL_GameController *gamepad;
int  dnGamepadConnected() {
    return (int)(gamepad != NULL);
}
extern "C" SDL_Haptic *haptic;
void dnVibrateGamepad(float force, int time) {
    if (dnGamepadConnected() && (haptic != NULL) && gamepadrumble) {
        SDL_HapticRumblePlay(haptic, force, time);
    }
}

int dnCanVibrate() {
#ifdef __APPLE__
    return 0;
#else
    return (int)(dnGamepadConnected() && (haptic != NULL));
#endif
}

void dnDrawQuad( int tid ) {
    static float coords[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    static float verts[]  = {-1, 1, 1, 1,-1,-1, 1,-1 };
    static float colors[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    
    glBindTexture( GL_TEXTURE_2D, tid );
    glVertexPointer( 2, GL_FLOAT, 0, verts );
    glTexCoordPointer( 2, GL_FLOAT, 0, coords );
    glColorPointer( 3, GL_FLOAT, 0, colors );
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
}

extern "C" {
    void Sys_ThrottleFPS(int max_fps);
    void getgamepad(void);
}

void dnInitVideoPlayback( int aspect ) {
    glEnable( GL_TEXTURE_2D );
    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glEnableClientState( GL_COLOR_ARRAY );

    glEnable( GL_BLEND );
    glDisable( GL_DEPTH_TEST );
    glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
    
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();

    switch ( aspect ) {
    case 1:
        glScalef( ( 16.0f / 9.0f ) / ( ScreenWidth / (float)ScreenHeight ), 1, 1 );
        break;
    case 2:
        glScalef( ( 320.0f / 200.0f ) / ( ScreenWidth / (float)ScreenHeight ), 1, 1 );
        break;
    case 3:
        glScalef( ( 4.0f / 3.0f ) / ( ScreenWidth / (float)ScreenHeight ), 1, 1 );
        break;
    default:
        break;
    }
    
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
}

void dnSwapBuffers( void ) {
    SDL_GL_SwapWindow( sdl_window );
}

void play_vpx_video(const char * filename, void (*frame_callback)(int)) {
    int ww = ScreenWidth;
    int hh = ScreenHeight;
    int frame = 0;
    
    SDL_ShowCursor(SDL_DISABLE);

    dnInitVideoPlayback( DN_VIDEO_16_9 );
        
    struct Vpxdata data;
    playvpx_init(&data,filename);
    int done = 0;
    while(playvpx_loop(&data) && !done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)){
            if (event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED) {
                getgamepad();
            }
            if (event.type == SDL_QUIT|| event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONDOWN)  {
                done = 1;
            }
        }
        GLuint tex = playvpx_get_texture(&data);
        if (!tex) { continue; }
        dnDrawQuad(tex);
        dnSwapBuffers();
        Sys_ThrottleFPS(40);
        glClear(GL_COLOR_BUFFER_BIT);
        frame++;
        if (frame_callback != NULL) {
            frame_callback(frame);
        }
        
    }
    
    playvpx_deinit(&data);
}
