#ifndef DN_MOUSE_INPUT
#define DN_MOUSE_INPUT

#if defined( __cplusplus )
extern "C" {
#endif

enum {
    DN_MOUSE_RAW = 0,
    DN_MOUSE_SDL = 1,
};

void dnSetMouseInputMode( int mode );
int  dnGetMouseInputMode( void );
void dnGetMouseInput( int *dx, int *dy, int peek );
void dnUpdateMouseInput( void );
void dnInitMouseInput( void );
void dnShutdownMouseInput( void );
int  dnGetNumberOfMouseInputDevices( void );

#if defined( __cplusplus )
}
#endif

#endif /* DN_MOUSE_INPUT */
