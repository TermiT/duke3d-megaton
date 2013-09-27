#include "OggPlayer.h"

static OggPlayer *oggPlayer = NULL;

extern "C" {

int  DSOP_Init(void *lpDS) {
	oggPlayer = new OggPlayer();
	if (oggPlayer != NULL) {
		oggPlayer->UseDirectSound((LPDIRECTSOUND8)lpDS);
		return 0;
	}
	return -1;
}

int  DSOP_Open(const char *filename) {
	return oggPlayer->OpenOgg(filename) ? 0 : -1;
}

void DSOP_Play(int loop) {
	oggPlayer->Play(loop != 0);
}

void DSOP_Stop() {
	oggPlayer->Stop();
}

void DSOP_Update() {
	if (oggPlayer != NULL)
		oggPlayer->Update();
}

int  DSOP_IsPlaying() {
	return oggPlayer->IsPlaying() ? 1 : 0;
}

void DSOP_Shutdown() {
	if (oggPlayer != NULL) {
		delete oggPlayer;
		oggPlayer = NULL;
	}
}

void DSOP_SetVolume(float volume) {
	if (oggPlayer != NULL) {
		oggPlayer->SetVolume(volume);
	}
}

}
