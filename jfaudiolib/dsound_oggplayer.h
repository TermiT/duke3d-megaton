#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

int  DSOP_Init(void *lpDS);
int  DSOP_Open(const char *filename);
void DSOP_Play(int loop);
void DSOP_Stop();
void DSOP_Update();
int  DSOP_IsPlaying();
void DSOP_Shutdown();
void DSOP_SetVolume(float volume);
