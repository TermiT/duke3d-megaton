//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef CSTEAM_H
#define CSTEAM_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CLOUD_FILES 41
#define MAX_CLOUD_FILE_LENGTH 15
#define MAX_CLOUD_FILE_SIZE 1024 * 1024  
    
int CSTEAM_Init(void);
void CSTEAM_Shutdown(void);
void CSTEAM_ShowOverlay(const char *dialog);
int CSTEAM_Online();
void CSTEAM_AchievementsInit();
int CSTEAM_GetStat(const char * statID);
void CSTEAM_SetStat(const char* statID, int number);
void CSTEAM_UnlockAchievement(const char * achievementID);
void CSTEAM_IndicateProgress(const char * achievementID, int currentNumber, int maxNumber);
void CSTEAM_UploadFile(const char * filename);
void CSTEAM_DownloadFile(const char * filename);
void CSTEAM_OpenCummunityHub(void);

#ifdef __cplusplus
}
#endif

#endif /* CSTEAM_H */
