//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "SDL.h"
#include "SDL_syswm.h"
#include "build.h"

int Win32_ShowErrorMessage(const char *caption, const char *message) {
    return MessageBoxA(0, message, caption, MB_OK|MB_ICONERROR);
}

static double PCFreq = 0.0;
static __int64 CounterStart = 0;

static LARGE_INTEGER m_high_perf_timer_freq;

void Win32_InitQPC() {
    if (!QueryPerformanceFrequency(&m_high_perf_timer_freq) ) {
        printf("QueryPerformanceFrequency failed!\n");
    }

    PCFreq = ( (double) (m_high_perf_timer_freq.QuadPart) ) /1000.0;

    QueryPerformanceCounter(&m_high_perf_timer_freq);
    CounterStart = m_high_perf_timer_freq.QuadPart;
}

double Win32_GetQPC() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return ( (double) (li.QuadPart-CounterStart) ) /PCFreq;
}

void Sys_ThrottleFPS(int max_fps) {
    static double end_of_prev_frame = 0.0;
    double frame_time, current_time, time_to_wait;
    int i;

    if (end_of_prev_frame != 0) {
        frame_time = 1000.0/max_fps;

        while (1) {
            current_time = Win32_GetQPC();
            time_to_wait = frame_time - (current_time - end_of_prev_frame);
            if (time_to_wait < 0.00001) {
                break;
            }
            if (time_to_wait > 2) {
                Sleep(1);
            } else {
                for (i = 0; i < 10; i++) {
                    Sleep(0);
                }
            }
        }

        end_of_prev_frame = current_time;
    } else {
        end_of_prev_frame = Win32_GetQPC();
    }
}

void Sys_InitTimer() {
    timeBeginPeriod(1);
    Win32_InitQPC();
}

void Sys_UninitTimer() {
    timeEndPeriod(1);
}

double Sys_GetTicks() {
    return Win32_GetQPC();
}

static
HWND GetHwnd() {
    struct SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

#if SDL_MAJOR_VERSION==1
    if (SDL_GetWMInfo(&wmInfo))
        return (HWND)wmInfo.window;
#else
    if (SDL_GetWindowWMInfo(sdl_window, &wmInfo) && wmInfo.subsystem == SDL_SYSWM_WINDOWS)
        return (HWND)wmInfo.info.win.window;
#endif

    return (HWND)0;
}

void*
Sys_GetWindow() {
    return (HWND)GetHwnd();
}

void Sys_GetScreenSize (int *width, int *height) {
    HMONITOR monitor = MonitorFromWindow(GetHwnd(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    *width = info.rcMonitor.right - info.rcMonitor.left;
    *height = info.rcMonitor.bottom - info.rcMonitor.top;
}

void Sys_CenterWindow (int w, int h) {
    HWND hwnd = GetHwnd();
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    RECT rc;
    int x, y;
    info.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &info);
    GetWindowRect(hwnd, &rc);
    x = info.rcWork.left + (info.rcWork.right-info.rcWork.left-w) /2;
    y = info.rcWork.top + (info.rcWork.bottom-info.rcWork.top-h) /2;
    MoveWindow(hwnd, x, y, w, h, FALSE);
}

void Sys_OutputDebugString (const char *string) {
    OutputDebugStringA(string);
}

void Sys_DPrintf(const char *format, ...) {
    char    buf[4096], *p = buf;
    va_list args;
    int     n;

    va_start(args, format);
    n = _vsnprintf(p, sizeof buf - 3, format, args); // buf-3 is room for CR/LF/NUL
    va_end(args);

    p += (n < 0) ? sizeof buf - 3 : n;

    while ( p > buf  &&  isspace(p[-1]) )
        *--p = '\0';

    *p++ = '\r';
    *p++ = '\n';
    *p   = '\0';

    OutputDebugString(buf);
	printf("%s", buf);
}

void Sys_Restart(const char *options) {
	char *cmd = "duke3d.exe ";

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

	strcat(cmd, options);
	CreateProcess( NULL,   // No module name (use command line)
        cmd,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi );           // Pointer to PROCESS_INFORMATION structure
    
    //WaitForSingleObject( pi.hProcess, INFINITE );

    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
	exit(0);
}

int Sys_FileExists( const char *path ) {
  DWORD dwAttrib = GetFileAttributes(path);

  return ((dwAttrib != INVALID_FILE_ATTRIBUTES && 
	  !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) ? 1 : 0;
}