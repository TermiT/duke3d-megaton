//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include <windows.h>
#include <stdio.h>

int Win32_ShowErrorMessage(const char *caption, const char *message) {
	return MessageBoxA(0, message, caption, MB_OK|MB_ICONERROR);
}

static double PCFreq = 0.0;
static __int64 CounterStart = 0;

void Win32_InitQPC() {
    LARGE_INTEGER li;
	if(!QueryPerformanceFrequency(&li)) {
		printf("QueryPerformanceFrequency failed!\n");
	}

    PCFreq = ((double)(li.QuadPart))/1000.0;

    QueryPerformanceCounter(&li);
    CounterStart = li.QuadPart;
}

double Win32_GetQPC() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return ((double)(li.QuadPart-CounterStart))/PCFreq;
}

