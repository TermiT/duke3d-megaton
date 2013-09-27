//
//  dn_system.h
//  duke3d
//
//  Created by Gennadiy Potapov on 16/8/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#ifndef duke3d_dn_system_h
#define duke3d_dn_system_h


#ifdef __cplusplus
extern "C" {
#endif
    
    int Sys_Init(int argc, char *argv[]);
    const char* Sys_GetResourceDir(void);
    void Sys_ShowAlert(const char * text);
    void Sys_StoreData(const char *filepath, const char *data);
    void Sys_InitTimer();
    void Sys_UninitTimer();
    double Sys_GetTicks();
    void Sys_ThrottleFPS(int max_fps);
    void Sys_GetScreenSize(int *width, int *height);
    void Sys_CenterWindow(int width, int height);
    void Sys_OutputDebugString(const char *string);
    
#ifdef __APPLE__
    int Sys_IsSnowLeopard();
#endif
    
#ifdef _WIN32
    void* win_gethwnd();
#endif
    
#ifdef __cplusplus
}
#endif

#endif


#endif
