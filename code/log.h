//
//  log.h
//  duke3d
//
//  Created by serge on 30.08.2014.
//  Copyright (c) 2014 General Arcade. All rights reserved.
//

#ifndef duke3d_log_h
#define duke3d_log_h

#if defined( __cplusplus )
extern "C" {
#endif

void Log_Open( void );
const char *Log_GetPath( void );
void Log_Printf( const char *format, ... );
void Log_Puts( const char *string );
void Log_Close( void );

#if defined( __cplusplus )
}
#endif
	
#endif
