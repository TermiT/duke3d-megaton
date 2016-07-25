//
//  log.cpp
//  duke3d
//
//  Created by serge on 30.08.2014.
//  Copyright (c) 2014 General Arcade. All rights reserved.
//

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "log.h"

static FILE *log = NULL;

const char* Log_GetPath( void ) {
	static char path[1024] = { 0 };
	if ( path[0] != 0 ) {
		return path;
	}
#if defined( _WIN32 )
	GetTempPathA( sizeof( path ), path );
#elif defined( __APPLE__ )
	strcpy( path, "/tmp/" );
#elif defined( __linux )
	strcpy( path, "/tmp/" );
#endif
	strcat( path, "duke.log" );
	return path;
};

void Log_Open( void ) {	
	if ( log != NULL ) {
		Log_Close();
	}
	const char *logPath = Log_GetPath();
	log = fopen( logPath, "wb+" );
	Log_Printf( "Started new log file\n" );
}

void Log_Printf( const char *format, ... ) {
	static char buffer[2048];
    va_list argptr;
    va_start( argptr, format );
	vsprintf( buffer, format, argptr );
    va_end( argptr );
	Log_Puts( buffer );
}

void Log_Puts( const char *string ) {
	if ( log != NULL ) {
		fputs( string, log );
	}
#if defined( _WIN32 )
	OutputDebugStringA( string );
#else
	puts( string );
#endif
}

void Log_Close( void ) {
	if ( log != NULL ) {
		fclose( log );
		log = NULL;
	}
}
