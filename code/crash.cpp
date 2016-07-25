//
//  crash.cpp
//  Duke Nukem 3D Megaton Edition
//
//  Created by serge on 26/08/14.
//  Copyright (c) 2014 generalarcade. All rights reserved.
//

#define USE_CRASH_HANDLER 1

#include "crash.h"
#include "log.h"

#if defined( _WIN32 ) && !defined( _DEBUG ) && USE_CRASH_HANDLER

#define WIN32_LEAN_AND_MEAN
#include "CrashRpt.h"

#include "dnAPI.h"
#include "helpers.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static
int CALLBACK crashCallback( CR_CRASH_CALLBACK_INFO *pInfo ) {
	Log_Close();
	return CR_CB_DODEFAULT;
}

void CrashHandler_Init() {
	Log_Open();
	OutputDebugStringA( "Installing CrashRpt handler\n" );
	CR_INSTALL_INFOA info = { 0 };
	info.cb = sizeof( CR_INSTALL_INFO );
	info.pszAppName = "Duke Nukem 3D Megaton Eition";
	//info.pszAppVersion = TOSTRING( BUILD_NUMBER );
	//strcpy( info.pszAppName, dnGetVersion() );
	info.pszAppName = dnGetVersion();
	info.pszEmailSubject = "DN3D:ME Crash Report";
	info.pszEmailTo = "";
	info.pszSmtpProxy = "";
	info.pszSmtpLogin = "";
	info.pszSmtpPassword = "";
	info.uPriorities[CR_SMTP] = 2;
	info.uPriorities[CR_SMAPI] = 1;
	info.dwFlags |= CR_INST_ALL_POSSIBLE_HANDLERS;
	info.dwFlags |= CR_INST_APP_RESTART;
	info.dwFlags |= CR_INST_SEND_QUEUED_REPORTS;
	info.pszRestartCmdLine = "";
	int nResult = crInstall( &info );
	if ( nResult != 0 ) {
		char msg[512] = { 0 };
		crGetLastErrorMsg( msg, sizeof( msg ) );
		OutputDebugString( va( "CrashRpt: %s\n", msg ) );
	}
	crSetCrashCallback( crashCallback, NULL );
	crAddFile2A( Log_GetPath(), "duke.log", "Debug log file", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK );
	crAddFile2A( "duke3d.cfg", "duke3d.cfg", "Config file", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK );
	crAddFile2A( "duke3d-local.cfg", "duke3d-local.cfg", "Local config file", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK );
	/*
	WCHAR path[MAX_PATH + 1];
	wsprintfW( path, L"%S", Log_GetFileName() );
	crAddFile2W( path, L"output.txt", L"Debug log file", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK );
	wsprintfW( path, L"%s/%S", MyPsVitaGame::GameData::getHomeDirW(), MyPsVitaGame::SaveData::getSaveFilename() );
	crAddFile2W( path, L"savegame.bin", L"Game data file", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK );
	*/
}

void CrashHandler_Free() {
//	Sys_Printf( "Removing CrashRpt handler\n" );
//	Log_Close();
	crUninstall();
}

#else

void CrashHandler_Init() {
	Log_Open();
}

void CrashHandler_Free() {
	Log_Close();
}

#endif
