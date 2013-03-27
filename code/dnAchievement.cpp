//
//  dnAchievement.cpp
//  duke3d
//
//  Created by termit on 1/17/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#include "dnAchievement.h"
#include "csteam.h"
#include "dnAPI.h"


void dnRecordDancerTip(){
    if (dnGetAddonId() != 0) return;
    CSTEAM_UnlockAchievement(ACHIEVEMENT_SHAKE_IT_BABY);
}

void dnHandleEndLevelAchievements(int Volume, int Level, int Time) {
    printf("Level: %d",Level);
    
	if (Volume == 0 && Level == 0 && (Time / (26*60)) < 3 && dnGetAddonId() == 0) {
        CSTEAM_UnlockAchievement(ACHIEVEMENT_THOSE_ALIEN_BASTARDS);
	}

    if (dnGetAddonId() == 0) {
        if (Volume == 0 && Level == 4) {
            CSTEAM_UnlockAchievement(ACHIEVEMENT_PIECE_OF_CAKE);
        } else if (Volume == 1 && Level == 8) {
            CSTEAM_UnlockAchievement(ACHIEVEMENT_LETS_ROCK);
        } else if (Volume == 2 && Level == 8) {
            CSTEAM_UnlockAchievement(ACHIEVEMENT_COME_GET_SOME);
        } else if (Volume == 3 && Level == 9) {
            CSTEAM_UnlockAchievement(ACHIEVEMENT_DAMN_IM_GOOD);
        }
    } else if (dnGetAddonId() == 1 && Volume == 2 && Level == 8){
        CSTEAM_UnlockAchievement(ACHIEVEMENT_PATRIOT);
    } else if (dnGetAddonId() == 2 && Volume == 1 && Level == 6){
        CSTEAM_UnlockAchievement(ACHIEVEMENT_BALLS_OF_SNOW);
    } else if (dnGetAddonId() == 3 && Volume == 2 && Level == 6){
        CSTEAM_UnlockAchievement(ACHIEVEMENT_LIMBO);
    }
}


void dnRecordEnemyStomp() {
    int number = CSTEAM_GetStat(STAT_THE_MIGHTY_FOOT);
	if (number > -1) {
		number++;
		CSTEAM_SetStat(STAT_THE_MIGHTY_FOOT, number);
		if ((number == 1 || number % 5 == 0) && number != ACHIEVEMENT_THE_MIGHTY_FOOT_MAX)  {
			CSTEAM_IndicateProgress(ACHIEVEMENT_THE_MIGHTY_FOOT, number, ACHIEVEMENT_THE_MIGHTY_FOOT_MAX);
		}
		if (number >= ACHIEVEMENT_THE_MIGHTY_FOOT_MAX) {
			CSTEAM_UnlockAchievement(ACHIEVEMENT_THE_MIGHTY_FOOT);
		}
	}
}

void dnRecordShitPile() {
    if (dnGetAddonId() != 0) return;
	CSTEAM_UnlockAchievement(ACHIEVEMENT_SHIT_HAPPENS);
}

void dnRecordSecret(int Volume, int Level, short SecretIndex) {
    int number = CSTEAM_GetStat(STAT_OOMP_UUGH_WHERE_IS_IT);
	if (number > -1) {
		number++;
		CSTEAM_SetStat(STAT_OOMP_UUGH_WHERE_IS_IT, number);
		if ((number == 1 || number % 10 == 0) && number != ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT_MAX)  {
			CSTEAM_IndicateProgress(ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT, number, ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT_MAX);
		}
		if (number >= ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT_MAX) {
			CSTEAM_UnlockAchievement(ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT);
		}
	}
}