//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "csteam.h"
//#include <steam_api.h>
#include <stdio.h>
#include <stdlib.h>
//#include "ISteamFriends.h"


char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH] = { 0 };

void dnAddCloudFileNames(void) {
    int count = 0;
    for (int i = 0; i < 10; i++) {
        for (int j=0; j < 4; j++) {
            sprintf(cloudFileNames[count], "game%d_%d.sav", i, j);
            count++;
        }
    }
}

extern "C"
int CSTEAM_Init(void) {
	printf("*** NOSTEAM Initialization ***\n");
    dnAddCloudFileNames();
    return 1;
}

extern "C"
void CSTEAM_Shutdown(void) {
	printf("*** NOSTEAM Shutdown ***\n");
}

extern "C"
void CSTEAM_ShowOverlay(const char *dialog) {
    printf("*** NOSTEAM ShowOverlay ***\n");
}

extern "C"
int CSTEAM_Online() {
	printf("*** NOSTEAM Online ***\n");
	return 1;
}

extern "C"
void CSTEAM_AchievementsInit() {
	printf("*** NOSTEAM AchievementsInit ***\n");
}

extern "C"
int CSTEAM_GetStat(const char * statID) {
	printf("*** NOSTEAM GetStat ***\n");
	return 0;
}

extern "C"
void CSTEAM_SetStat(const char* statID, int number) {
	printf("*** NOSTEAM SetStat ***\n");
}

extern "C"
void CSTEAM_UnlockAchievement(const char * achievementID) {
   printf("*** NOSTEAM UnlockAchievement ***\n");
}

extern "C"
void CSTEAM_IndicateProgress(const char * achievementID, int currentNumber, int maxNumber) {
	printf("*** NOSTEAM IndicateProgress ***\n");
}

extern "C"
void CSTEAM_DownloadFile(const char * filename) {
}

extern "C"
void CSTEAM_UploadFile(const char * filename) {
};

extern "C"
void CSTEAM_OpenCummunityHub(void) {
    printf("*** NOSTEAM OpenCummunityHub ***\n");
}
