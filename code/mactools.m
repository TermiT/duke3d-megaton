//
//  mactools.m
//  duke3d
//
//  Created by termit on 1/22/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#include <mach/mach_time.h>
#include <sys/time.h>


#define MAX_PATH 2048



const char* Sys_GetResourceDir(void) {
    static char data[MAX_PATH] = { 0 };
    NSString * path = [[NSBundle mainBundle].resourcePath stringByAppendingPathComponent:@"gameroot"];
    if (!data[0]) {
        strcpy(data, path.UTF8String);
    }
    return data;
}

void Sys_ShowAlert(const char * text) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:text]];
        [alert setInformativeText:@"Duke Nukem"];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    });
}

void Sys_StoreData(const char *filepath, const char *data) {
    FILE *fp = fopen(filepath, "ab");
    if (fp != NULL)
    {
        fputs(data, fp);
        fclose(fp);
    }
}

static
NSString * Sys_GetOSVersion() {
    NSProcessInfo *pInfo = [NSProcessInfo processInfo];
    NSString *version = [pInfo operatingSystemVersionString];
    return version;
}

bool Sys_IsSnowLeopard() {
    return (bool)([Sys_GetOSVersion() rangeOfString:@"10.6"].location != NSNotFound);
}

static uint64_t timer_start;

void Sys_InitTimer() {
    timer_start = mach_absolute_time();
}

void Sys_UninitTimer() {
    
}

static
double subtractTimes( uint64_t endTime, uint64_t startTime )
{
    uint64_t difference = endTime - startTime;
    static double conversion = 0.0;
    
    if( conversion == 0.0 )
    {
        mach_timebase_info_data_t info;
        kern_return_t err = mach_timebase_info( &info );
        
        //Convert the timebase into milliseconds
        if( err == 0  )
            conversion = 1e-6 * (double) info.numer / (double) info.denom;
    }
    
    return conversion * (double) difference;
}

double Sys_GetTicks() {
    uint64_t current;
    current = mach_absolute_time();
    return subtractTimes(current, timer_start);
}

int Sys_FileExists( const char *path ) {
	return access( path, F_OK ) != -1 ? 1 : 0;
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
    NSRect screenRect = [[NSScreen mainScreen] frame];
    *width = (int) screenRect.size.width;
    *height = (int) screenRect.size.height;
}

void Sys_CenterWindow(int width, int height) {
    
}

void Sys_OutputDebugString (const char *string) {
    puts( string );
}

void Sys_DPrintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    vprintf(format, args);
    
    va_end(args);
}

void Sys_Restart(const char *options) {
    NSTask *task = [[NSTask alloc] init];
    NSBundle *bundle = [NSBundle bundleWithPath:[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"../duke3d.app"]];
    [task setLaunchPath:[bundle executablePath]];
    NSArray *arguments = [[NSString stringWithUTF8String:options] componentsSeparatedByString:@" "];
    [task setArguments:arguments];
    [task launch];
    exit(0);
}
