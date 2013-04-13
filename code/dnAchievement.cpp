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


void dnRecordCountAchievement(const char* achievement, const char* stat, int max_count, int show_progress_every) {
    int number = CSTEAM_GetStat(stat);
	if (number > -1) {
		number++;
		CSTEAM_SetStat(stat, number);
		if ((number == 1 || number % show_progress_every == 0) && number != max_count)  {
			CSTEAM_IndicateProgress(achievement, number, max_count);
		}
		if (number >= max_count) {
			CSTEAM_UnlockAchievement(achievement);
		}
	}
}

void dnRecordDancerTip(){
    if (dnGetAddonId() != 0) return;
    CSTEAM_UnlockAchievement(ACHIEVEMENT_SHAKE_IT_BABY);
}

void dnHandleEndLevelAchievements(int Volume, int Level, int Time, int MaxKills, int Kills) {
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
    
    if (Kills >= MaxKills) {
        CSTEAM_UnlockAchievement(ACHIEVEMENT_TAKE_NO_PRISONERS);
    }
}

void dnRecordEnemyKilled() {
    dnRecordCountAchievement(ACHIEVEMENT_INVADERS_MUST_DIE, STAT_INVADERS_MUST_DIE, ACHIEVEMENT_INVADERS_MUST_DIE_MAX, 10);
}


void dnRecordEnemyStomp() {
    dnRecordCountAchievement(ACHIEVEMENT_THE_MIGHTY_FOOT, STAT_THE_MIGHTY_FOOT, ACHIEVEMENT_THE_MIGHTY_FOOT_MAX, 5);
}

void dnRecordShitPile() {
    if (dnGetAddonId() != 0) return;
	CSTEAM_UnlockAchievement(ACHIEVEMENT_SHIT_HAPPENS);
}

void dnRecordSecret(int Volume, int Level, short SecretIndex) {
    //printf("volum %d, level %d, secret index: %d\n", Volume, Level, SecretIndex);
    //doom marine: 0, 2, 3
    dnRecordCountAchievement(ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT, STAT_OOMP_UUGH_WHERE_IS_IT, ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT_MAX, 10);
}


