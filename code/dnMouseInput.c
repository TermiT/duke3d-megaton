#include <stdio.h>
#include <SDL.h>
#include "dnMouseInput.h"
#include "manymouse.h"

static int s_numDevices;
static int s_mode;
static int s_dx, s_dy;
#if defined( _WIN32 )
static int s_mouseParkingX, s_mouseParkingY;
#endif

void Sys_OutputDebugString( const char *string );
const char* va( const char *format, ... );
extern int norawmouse;

void dnSetMouseInputMode( int mode ) {
    s_mode = mode;
    s_dx = 0;
    s_dy = 0;
#if defined( _WIN32 )
    if ( mode == DN_MOUSE_RAW ) {
        SDL_GetMouseState( &s_mouseParkingX, &s_mouseParkingY );
    }
#endif
}

int dnGetMouseInputMode( void ) {
    return s_mode;
}

void dnGetMouseInput( int *dx, int *dy, int peek ) {
    if ( s_mode == DN_MOUSE_RAW ) {
        *dx = s_dx;
        *dy = s_dy;
        if ( !peek ) {
            s_dx = 0;
            s_dy = 0;
        }
#if defined( _WIN32 )
        SDL_WarpMouseInWindow( NULL, s_mouseParkingX, s_mouseParkingY );
#endif
    } else {
        *dx = 0;
        *dy = 0;
    }
}

void dnUpdateMouseInput( void ) {
    ManyMouseEvent event;
//    s_dx = 0;
//    s_dy = 0;
    while ( ManyMouse_PollEvent( &event ) ) {
        if ( event.device > s_numDevices ) {
            continue;
        }
        if ( event.type == MANYMOUSE_EVENT_RELMOTION ) {
            if ( event.item == 0 ) {
                s_dx += event.value;
            } else if ( event.item == 1 ) {
                s_dy += event.value;
            }
        }
    }
}

static void printInfo( void ) {
    int i;
    Sys_OutputDebugString( va( "Mouse input diagnostic info\n" ) );
    Sys_OutputDebugString( va( "Driver name: %s\n", ManyMouse_DriverName() ) );
    Sys_OutputDebugString( va( "Number of devices: %d\n", s_numDevices ) );
    for ( i = 0; i < s_numDevices; i++ ) {
        Sys_OutputDebugString( va( "Device #%d: %s\n", i, ManyMouse_DeviceName( i ) ) );
    }
}

void dnInitMouseInput( void ) {
    s_numDevices = ManyMouse_Init();
#if 1
    printInfo();
#endif
    if ( norawmouse ) {
        s_numDevices = 0;
        Sys_OutputDebugString( "ManyMouse has been disabled by command line option\n" );
    }
}

void dnShutdownMouseInput( void ) {
    ManyMouse_Quit();
}

int  dnGetNumberOfMouseInputDevices( void ) {
    return s_numDevices;
}
