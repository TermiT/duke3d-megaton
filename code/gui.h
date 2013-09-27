//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef GUI_H
#define GUI_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

void GUI_Init(int width, int height);
void GUI_Shutdown(void);
void GUI_Render(void);
void GUI_PreModeChange(void);
void GUI_PostModeChange(int width, int height);
void GUI_Enable(int enabled);
int  GUI_IsEnabled(void);
int  GUI_InjectEvent(SDL_Event *ev);
void GUI_TimePulse(void);
void GUI_EnableMenuForCurrentFrame(void);
void GUI_ShowSaveMenu(void);
void GUI_ShowLoadMenu(void);
void GUI_ShowHelpMenu(void);
void GUI_ShowSoundMenu(void);
void GUI_ShowVideoSettingsMenu(void);
void GUI_ShowGameOptionsMenu(void);
void GUI_ShowQuitConfirmation(void);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
