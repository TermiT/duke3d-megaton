//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include <map>
#include <set>
#include <algorithm>
#include <stdio.h>

#include "dnAPI.h"
#include "build.h"
#include "SDL.h"
#include "helpers.h"
#include "csteam.h"

void dnGetVideoModeList(VideoModeList& modes) {
	typedef std::pair<int, int> ScreenSize;
	typedef std::pair<ScreenSize, int> ScreenMode;
	typedef std::set<ScreenMode> ModeSet;
	ModeSet mode_set;

	for (int i = 0; i < validmodecnt; i++) {
		int bpp_fs = SDL_VideoModeOK(validmode[i].xdim, validmode[i].ydim, 32, SDL_OPENGL|SDL_FULLSCREEN);
		int bpp_win = SDL_VideoModeOK(validmode[i].xdim, validmode[i].ydim, 32, SDL_OPENGL);
		if (bpp_fs == bpp_win && (bpp_fs == 24 || bpp_fs == 32)) {
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
		int bpp = i->second;
		vm.width = size.first;
		vm.height = size.second;
		vm.bpp = i->second;
		vm.fullscreen = 0;
		modes.push_back(vm);
	}
}

extern "C" long setgamemode(char davidoption, long daxdim, long daydim, long dabpp);
extern "C" void polymost_glreset ();

void dnChangeVideoMode(VideoMode *v) {
	//setvideomode(videomode->width, videomode->height, videomode->bpp, videomode->fullscreen);
	//resetvideomode();
	polymost_glreset();
	setgamemode(v->fullscreen, v->width, v->height, v->bpp);
}

bool operator == (const VideoMode& a, const VideoMode& b) {
	return (a.bpp==b.bpp) && (clamp(a.fullscreen,0,1)==clamp(b.fullscreen,0,1)) && (a.height==b.height) && (a.width==b.width);
}

void dnPullCloudFiles() {
    extern char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH];
	for (int i = 0; i < MAX_CLOUD_FILES; i++) {
		CSTEAM_DownloadFile(cloudFileNames[i]);
	}
}

void dnPushCloudFiles() {
    extern char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH];
	for (int i = 0; i < MAX_CLOUD_FILES; i++) {
		CSTEAM_UploadFile(cloudFileNames[i]);
	}
}