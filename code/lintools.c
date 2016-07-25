//
//  lintools.m
//  duke3d
//
//  Created by termit on 1/22/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//


#include <X11/Xlib.h>
#include <stdio.h>
#include <time.h>
#include <SDL/SDL.h>
#include <unistd.h>

#define MAX_PATH 2048

const char* Sys_GetResourceDir(void) {
}

void Sys_ShowAlert(const char * text) {
}

void Sys_StoreData(const char *filepath, const char *data) {
    FILE *fp = fopen(filepath, "ab");
    if (fp != NULL)
    {
        fputs(data, fp);
        fclose(fp);
    }
}

double CountTicks() {
    double ti;
    struct timeval tv;
    if (gettimeofday(&tv,NULL) < 0) return 0;
    // tv is sec.usec, GTC gives msec
    ti = tv.tv_sec * 1000;
    ti += tv.tv_usec / 1000.0;
    //printf("Timer: %f\n", ti);
    return ti;
}


static double start_timer;
void Sys_InitTimer() {
    start_timer = CountTicks();
}

void Sys_UninitTimer() {
    
}


double Sys_GetTicks() {
#ifdef USE_SDL_TIMER
	return SDL_GetTicks();
#else
    return CountTicks() - start_timer;
#endif
}


void Sys_ThrottleFPS(int max_fps) {
	static int throttler_ready = 0;
	static double end_of_prev_frame;
	double frame_time, current_time, time_to_wait;
    struct timespec ts = { 0 };
    
	if (throttler_ready) {
		frame_time = 1000.0/max_fps;

        do {
            current_time = Sys_GetTicks();
            time_to_wait = frame_time - (current_time - end_of_prev_frame);
                        
            if (time_to_wait > 0) {
                ts.tv_sec = 0;
                ts.tv_nsec = (long)(time_to_wait*1000000.0);
                nanosleep(&ts, NULL);
            }
        } while (0);
        
	} else {
        throttler_ready = 1;
	}
    end_of_prev_frame = Sys_GetTicks();

}

void Sys_GetScreenSize(int *width, int *height) {
	Display* pdsp = NULL;
 	Screen* pscr = NULL;

 	pdsp = XOpenDisplay( NULL );
 	if (!pdsp ) {
  		fprintf(stderr, "Failed to open default display.\n");
  		return;
 	}

    pscr = DefaultScreenOfDisplay( pdsp );
 	if (!pscr) {
  		fprintf(stderr, "Failed to obtain the default screen of given display.\n");
  		return ;
 	}

 	*width = pscr->width;
 	*height = pscr->height;

 	XCloseDisplay( pdsp );
}

void Sys_CenterWindow(int width, int height) {
    
}

void Sys_DPrintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    vprintf(format, args);
    
    va_end(args);
}

void Sys_OutputDebugString (const char *string) {
    puts( string );
}


void Sys_Restart(const char *options) {
   execl("../bin/duke3d.sh", options, NULL);
   exit(0);
}

int Sys_FileExists( const char *path ) {
    return access( path, F_OK ) != -1 ? 1 : 0;
}
