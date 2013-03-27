//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

//#define NOGUI

#ifdef NOGUI

#include <SDL.h>

extern "C" {
	void GUI_Init(int width, int height){}
	void GUI_Shutdown(void){}
	void GUI_Render(void){}
	void GUI_PreModeChange(void){}
	void GUI_PostModeChange(int width, int height){}
	void GUI_Enable(int enabled){}
	int  GUI_InjectEvent(SDL_Event *ev) { return 0; }
	int  GUI_IsEnabled(void) { return 0; }
	void GUI_TimePulse(void){}
}
#else

#include <stdlib.h>
#include <assert.h>
#include "gui_private.h"

GUI *gui = NULL;

extern "C"
void GUI_Init(int width, int height) {
	assert(gui == NULL);
	gui = new GUI(width, height);
}

extern "C"
void GUI_Shutdown(void) {
	assert(gui != NULL);
	delete gui;
	gui = NULL;
}

extern "C"
void GUI_Render(void) {
	assert(gui != NULL);
	gui->Render();
}

static int modeChangeInProgress = 0;

extern "C"
void GUI_PreModeChange(void) {
	if (gui != NULL && !modeChangeInProgress) {
		gui->PreModeChange();
		modeChangeInProgress = 1;
	}
}

extern "C"
void GUI_PostModeChange(int width, int height) {
	if (gui != NULL && modeChangeInProgress) {
		gui->PostModeChange(width, height);
		modeChangeInProgress = 0;
	}
}

extern "C"
void GUI_Enable(int enabled) {
	assert(gui != NULL);
	gui->Enable(enabled?true:false);
}

extern "C"
int GUI_InjectEvent(SDL_Event *ev) {
	assert(gui != NULL);
	return (int)gui->InjectEvent(ev);
}

extern "C"
int  GUI_IsEnabled(void) {
	assert(gui != NULL);
	return gui->IsEnabled()?1:0;
}

extern "C"
void GUI_TimePulse(void) {
	assert(gui != NULL);
	return gui->TimePulse();
}

extern "C"
void GUI_EnableMenuForCurrentFrame(void) {
	assert(gui != NULL);
	gui->EnableForCurrentFrame();
}

#endif
