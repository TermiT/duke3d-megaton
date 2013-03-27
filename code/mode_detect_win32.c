#include <windows.h>
#include "SDL.h"
#include "SDL_syswm.h"

static
HWND GetHwnd() {
     SDL_SysWMinfo wmi;
     SDL_VERSION(&wmi.version);

	 if(SDL_GetWMInfo(&wmi)) {
		 return (HWND)wmi.window;
	 }
	 return (HWND)0;
}

void dnGetScreenSize(int *width, int *height) {
	HMONITOR monitor = MonitorFromWindow(GetHwnd(), MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	*width = info.rcMonitor.right - info.rcMonitor.left;
	*height = info.rcMonitor.bottom - info.rcMonitor.top;
}
