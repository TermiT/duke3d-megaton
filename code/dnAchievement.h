//
//  dnAchievement.h
//  duke3d
//
//  Created by termit on 1/17/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#ifndef duke3d_dnAchievement_h
#define duke3d_dnAchievement_h

#define ACHIEVEMENT_PIECE_OF_CAKE                   "ACHIEVEMENT_PIECE_OF_CAKE"
#define ACHIEVEMENT_LETS_ROCK                       "ACHIEVEMENT_LETS_ROCK"
#define ACHIEVEMENT_COME_GET_SOME                   "ACHIEVEMENT_COME_GET_SOME"
#define ACHIEVEMENT_DAMN_IM_GOOD                    "ACHIEVEMENT_DAMN_IM_GOOD"
#define ACHIEVEMENT_THE_MIGHTY_FOOT                 "ACHIEVEMENT_THE_MIGHTY_FOOT"
#define ACHIEVEMENT_SHIT_HAPPENS                    "ACHIEVEMENT_SHIT_HAPPENS"
#define ACHIEVEMENT_SHAKE_IT_BABY                   "ACHIEVEMENT_SHAKE_IT_BABY"
#define ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT           "ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT"
#define ACHIEVEMENT_THOSE_ALIEN_BASTARDS            "ACHIEVEMENT_THOSE_ALIEN_BASTARDS"

#define ACHIEVEMENT_PATRIOT                         "ACHIEVEMENT_PATRIOT"
#define ACHIEVEMENT_BALLS_OF_SNOW                   "ACHIEVEMENT_BALLS_OF_SNOW"
#define ACHIEVEMENT_LIMBO                           "ACHIEVEMENT_LIMBO"

#define ACHIEVEMENT_TAKE_NO_PRISONERS               "ACHIEVEMENT_TAKE_NO_PRISONERS"
#define ACHIEVEMENT_INVADERS_MUST_DIE               "ACHIEVEMENT_INVADERS_MUST_DIE"

#define ACHIEVEMENT_SHAME                           "ACHIEVEMENT_SHAME"
#define ACHIEVEMENT_DOOMED_MARINE                   "ACHIEVEMENT_DOOMED_MARINE"
#define ACHIEVEMENT_USE_THE_FORCE                   "ACHIEVEMENT_USE_THE_FORCE"

#define STAT_THE_MIGHTY_FOOT						"STAT_THE_MIGHTY_FOOT"
#define STAT_OOMP_UUGH_WHERE_IS_IT					"STAT_OOMP_UUGH_WHERE_IS_IT"
#define STAT_INVADERS_MUST_DIE                      "STAT_INVADERS_MUST_DIE"


//MULTIPLAYER
#define ACHIEVEMENT_MP_TIER1                        "ACHIEVEMENT_MP_TIER1"
#define ACHIEVEMENT_MP_TIER2                        "ACHIEVEMENT_MP_TIER2"
#define ACHIEVEMENT_MP_TIER3                        "ACHIEVEMENT_MP_TIER3"

#define STAT_MP                                     "STAT_MP"

//#define ACHIEVEMENT_BLOW_IT_OUT_YOUR_ASS            "ACHIEVEMENT_BLOW_IT_OUT_YOUR_ASS"
//#define ACHIEVEMENT_IVE_GOT_BALLS_OF_STEEL          "ACHIEVEMENT_IVE_GOT_BALLS_OF_STEEL"
//#define ACHIEVEMENT_HAIL_TO_THE_KING_BABY           "ACHIEVEMENT_HAIL_TO_THE_KING_BABY"


#define ACHIEVEMENT_THE_MIGHTY_FOOT_MAX				40
#define ACHIEVEMENT_OOMP_UUGH_WHERE_IS_IT_MAX		70
#define ACHIEVEMENT_INVADERS_MUST_DIE_MAX           1000

#define ACHIEVEMENT_MP_TIER1_MAX                    100
#define ACHIEVEMENT_MP_TIER2_MAX                    250
#define ACHIEVEMENT_MP_TIER3_MAX                    500



#ifdef __cplusplus
extern "C" {
#endif
    
void dnRecordDancerTip();
void dnHandleEndLevelAchievements(int Volume, int Level, int Time, int MaxKills, int Kills);
void dnRecordEnemyStomp();
void dnRecordShitPile();
void dnRecordSecret(int Volume, int Level, short SecretIndex);
void dnFoundSecret(short sectorindex);
void dnRecordEnemyKilled();
void dnRecordCheat();
void dnRecordDukeTalk(short num);
void dnRecordMultiplayerKill();
    
    
#ifdef __cplusplus
}
#endif


#endif
