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

void dnRecordDancerTip() {
    if (dnGetAddonId() != 0 || ud.multimode > 1) return;
    CSTEAM_UnlockAchievement(ACHIEVEMENT_SHAKE_IT_BABY);
}

void dnRecordMultiplayerKill() {
    if (ud.coop == 1) return;
    int number = CSTEAM_GetStat(STAT_MP);
    if (number > -1) {
        number++;
        CSTEAM_SetStat(STAT_MP, number);
        if (number == 1 || (number < ACHIEVEMENT_MP_TIER1_MAX && number % 10 == 0)) {
            CSTEAM_IndicateProgress(ACHIEVEMENT_MP_TIER1, number, ACHIEVEMENT_MP_TIER1_MAX);
        } else if (number >= ACHIEVEMENT_MP_TIER1_MAX && number < ACHIEVEMENT_MP_TIER2_MAX &&  number % 25 == 0) {
            CSTEAM_IndicateProgress(ACHIEVEMENT_MP_TIER2, number, ACHIEVEMENT_MP_TIER2_MAX);
            if (number == ACHIEVEMENT_MP_TIER1_MAX) {
                CSTEAM_UnlockAchievement(ACHIEVEMENT_MP_TIER1);
            }
        } else if (number >= ACHIEVEMENT_MP_TIER2_MAX && number % 50 == 0) {
            CSTEAM_IndicateProgress(ACHIEVEMENT_MP_TIER3, number, ACHIEVEMENT_MP_TIER3_MAX);
            if (number == ACHIEVEMENT_MP_TIER2_MAX) {
                CSTEAM_UnlockAchievement(ACHIEVEMENT_MP_TIER2);
            } else if (number == ACHIEVEMENT_MP_TIER3_MAX){
                CSTEAM_UnlockAchievement(ACHIEVEMENT_MP_TIER3);
            }
        }
    }
}

void dnHandleEndLevelAchievements(int Volume, int Level, int Time, int MaxKills, int Kills) {
    if (ud.multimode > 1) return;    
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
    if (ud.multimode > 1) return;
    dnRecordCountAchievement(ACHIEVEMENT_INVADERS_MUST_DIE, STAT_INVADERS_MUST_DIE, ACHIEVEMENT_INVADERS_MUST_DIE_MAX, 10);
}


void dnRecordEnemyStomp() {
    if (ud.multimode > 1) return;
    dnRecordCountAchievement(ACHIEVEMENT_THE_MIGHTY_FOOT, STAT_THE_MIGHTY_FOOT, ACHIEVEMENT_THE_MIGHTY_FOOT_MAX, 5);
}

void dnRecordShitPile() {
    if (dnGetAddonId() != 0 || ud.multimode > 1) return;
	CSTEAM_UnlockAchievement(ACHIEVEMENT_SHIT_HAPPENS);
}

void dnRecordSecret(int Volume, int Level, short SecretIndex) {
    if (ud.multimode > 1) return;
    //printf("volum %d, level %d, secret index: %d\n", Volume, Level, SecretIndex);
    //doom marine: 0, 2, 3
    dnRecordCountAchievement(ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT, STAT_OOMP_UUGH_WHERE_IS_IT, ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT_MAX, 10);
}

void dnRecordCheat() {
    if (ud.multimode > 1) return;
    CSTEAM_UnlockAchievement(ACHIEVEMENT_SHAME);
}

void dnRecordDukeTalk(short num) {
    if (dnGetAddonId() != 0 || ud.multimode > 1) return;
//            printf("NUM %d", num);
    if (num == 193) { //Doomed space marine
        CSTEAM_UnlockAchievement(ACHIEVEMENT_DOOMED_MARINE);
    } else if (num == 235) { // Skywalker
        CSTEAM_UnlockAchievement(ACHIEVEMENT_USE_THE_FORCE);
    }
}

