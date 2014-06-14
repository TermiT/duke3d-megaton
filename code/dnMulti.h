//
//  dnMulti.h
//  duke3d
//
//  Created by serge on 18/01/14.
//  Copyright (c) 2014 General Arcade. All rights reserved.
//

#ifndef duke3d_dnMulti_h
#define duke3d_dnMulti_h

/*
 
 Duke Nukem 3D Megaton Edition
 Multiplayer routines
 
 Steam supports multi-channel transmission. So I use different channels for different purposes:
	Channel 0: legacy DN3D network communications
	Channel 1: pong response
	Channel 2: ping request, client-server synchronizations, snapshot transmission, etc.
 
 */

#ifdef __cplusplus
extern "C" {
#endif
	
#include "duke3d.h"
#include "csteam.h"

#pragma pack(push,1)
	
#define NET_RELIABLE 0
	
enum {
	CHAN_LEGACY = 0,
	CHAN_PONG = 1,
	CHAN_SYNC = 2,
};
	
enum {
	TAG_PING,
	TAG_READY,
	TAG_PREMATCH,
	TAG_BLOCK_CHUNK,
	TAG_BLOCK_STATUS,
	TAG_NOTIFICATION,
	TAG_QUIT,
};
	
typedef enum {
	QUIT_LEFT = 0,
	QUIT_KICKED = 1,
	QUIT_SPLIT = 2,
} quitReason_t;
	
typedef enum {
	BLOCK_NONE = 0,
	BLOCK_SNAPSHOT = 1,
} blockKind_t;
	
typedef enum {
	NOTIFICATION_OUT_OF_SYNC,
	NOTIFICATION_FORCE_SYNC,
} notification_t;
	
typedef struct {
	int sessionToken;
	unsigned short tag;
} packetHeader_t;
	
typedef struct {
	packetHeader_t header;
	int value;
} pingPacket_t;

typedef struct {
	unsigned char aimMode, autoAim, weaponswitch;
	unsigned char wchoice[MAX_WEAPONS];
} playerPrefs_t;
	
typedef struct {
	packetHeader_t header;
	playerPrefs_t playerPrefs;
} readyPacket_t;

typedef struct {
	packetHeader_t header;
	int timeToStart;
	unsigned char readyPlayers;
	playerPrefs_t playerPrefs[MAXPLAYERS];
} prematchStatusPacket_t;
	
typedef struct {
	packetHeader_t header;
	unsigned int blockSize;
	unsigned int allBlockChunks;
	unsigned int chunkMask;
	unsigned int chunkIndex;
	unsigned int chunkOffset, chunkSize;
	unsigned char chunkData[1];
} blockChunk_t;
	
typedef struct {
	packetHeader_t header;
	unsigned int blockID;
	unsigned int chunkMask;
} blockDownloadStatus_t;
	
typedef struct {
	packetHeader_t header;
	notification_t notification;
} notificationPacket_t;

typedef struct {
	packetHeader_t header;
	quitReason_t reason;
} quitPacket_t;
	
#pragma pack(pop)
	
void dnGetPackets( void );
void dnWaitForEverybody( blockKind_t blockKind );
int  dnPing( steam_id_t remote );
int  dnCheckPong( steam_id_t remote, int value );

int  dnEnterMultiMode( const lobby_info_t *lobbyInfo );
void dnExitMultiMode( void );
int  dnIsInMultiMode( void );	
int  dnIsHost( void );
void dnRemovePlayer( int playerIndex );
	
void dnNotifyPlayer( int playerIndex, notification_t notification );
	
int dnSendPacket( int playerIndex, const void *bufptr, int size );
int dnGetPacket( int *playerIndex, void *bufptr, int size );
	
void dnResyncIfNeeded( void );
	
int  dnIsPlayerIndexValid( int playerIndex );
int  dnGetNextPlayer( int playerIndex );
	
	
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#define dnIterClients(varname) for ( int varname = dnGetNextPlayer(0); varname >= 0; varname = dnGetNextPlayer( varname ) )
#define dnIterPlayers(varname) for ( int varname = 0; varname >= 0; varname = dnGetNextPlayer( varname ) )

#else

#define dnIterClients(varname) for ( varname = dnGetNextPlayer(0); varname >= 0; varname = dnGetNextPlayer( varname ) )
#define dnIterPlayers(varname) for ( varname = 0; varname >= 0; varname = dnGetNextPlayer( varname ) )

#endif


#endif
