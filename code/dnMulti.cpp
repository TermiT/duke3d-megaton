//
//  dnMulti.cpp
//  duke3d
//
//  Created by serge on 18/01/14.
//  Copyright (c) 2014 General Arcade. All rights reserved.
//

#include <stdio.h>
#include "csteam.h"
#include "dnAPI.h"

extern "C" {
#include "types.h"
#include "gamedefs.h"
#include "function.h"
#include "config.h"
#include "build.h"
#include "duke3d.h"
#include "mmulti.h"
#include "crc32.h"
}

#ifndef _WIN32
#include <unistd.h>
#endif

#include "dnMulti.h"
#include "dnSnapshot.h"

#define MAX_PACKET_SIZE (1024*1024)
#define MAX_BLOCK_SIZE (5*1024*1024)
#define BLOCK_CHUNK_SIZE (MAX_PACKET_SIZE - 5*1024)
#define CHUNKS_AT_TIME (1)

#define READY_RESEND_DELAY (1000.0)
#define PREMATCH_STATUS_RESEND_DELAY (2000.0)
#define WFE_DELAY (30000.0)
#define ALLCLEAR_DELAY (3000.0)
#define BLOCK_REQUEST_DELAY (500.0)

static unsigned char packetBuffer[MAX_PACKET_SIZE];

static int sessionToken = 0xDEADBEEF;
static steam_id_t playerIDs[MAXPLAYERS];
static int numPlayers, numActivePlayers;
static int myConnectIndex;

static unsigned char readyPlayers;
static playerPrefs_t playerPrefs[MAXPLAYERS];
static int timeToStart;

static struct {
	unsigned char buffer[MAX_BLOCK_SIZE];
	unsigned int size;
	unsigned int chunkMask;
	int transmissionInProgress;
} block = { 0 };

static blockDownloadStatus_t clientBlockStatus[MAXPLAYERS];

static snapshot_t snapshot;
static bool doLoadSnapshot;

static double lastReadTimes[MAXPLAYERS];
static bool timeoutCheckEnabled;

enum {
	STATE_INACTIVE,
	STATE_DOWNLOADING,
	STATE_PREMATCH,
	STATE_AWAITING_MATCH,
} wfeState = STATE_INACTIVE;

static char waitStatus[100] = { 0 };

static bool awaitingResync = false;

static
void dnEnableTimeouts( void ) {
	double now = Sys_GetTicks();
	dnIterPlayers( i ) {
		lastReadTimes[i] = now;
	}
	timeoutCheckEnabled = true;
}

static
void dnDisableTimeouts( void ) {
	timeoutCheckEnabled = false;
}

static
void dnCheckTimeouts( void ) {
	
}

static
void dnSendQuit( int playerIndex, quitReason_t quitReason ) {
	if ( dnIsPlayerIndexValid( playerIndex ) ) {
		quitPacket_t quitPacket;
		
		quitPacket.header.sessionToken = sessionToken;
		quitPacket.header.tag = TAG_QUIT;
		quitPacket.reason = quitReason;
		
		CSTEAM_SendPacket( playerIDs[playerIndex], &quitPacket, sizeof( quitPacket_t ), 1, CHAN_SYNC );
	}
}

static
void dnUpdateActivePlayers( void ) {
	numActivePlayers = 0;
	for ( int i = 0; i < numPlayers; i++ ) {
		if ( playerIDs[i] != 0 ) {
			numActivePlayers++;
		}
	}
}

extern "C"
int  dnIsPlayerIndexValid( int playerIndex ) {
	bool result = false;
	
	if ( playerIndex >= 0 && playerIndex < numPlayers ) {
		if ( playerIDs[playerIndex] != 0 ) {
			result = true;
		} else {
//			Sys_DPrintf( "[DUKEMP] playerIndex is out of range\n" );
		}
	} else {
//		Sys_DPrintf( "[DUKEMP] player steam id is invalid\n");
	}
	return result;
}

extern "C"
int  dnGetNextPlayer( int playerIndex ) {
	int result = -1;
	do {
		playerIndex++;
	} while ( !dnIsPlayerIndexValid( playerIndex ) && playerIndex < numPlayers );
	if ( dnIsPlayerIndexValid( playerIndex ) ) {
		result = playerIndex;
	}
	return result;
}

static
int dnNumBitsSet( unsigned int mask ) {
	int result = 0;
	for ( int i = 0; i < 32; i++ ) {
		if ( mask & ( 1 << i ) ) {
			result++;
		}
	}
	return result;
}

static
void dnDrawLoadingScreen( const char *statusText ) {
	int i = ud.screen_size;
    ud.screen_size = 0;
    dofrontscreens( statusText );
    vscrn();
    ud.screen_size = i;
}

static
void dnDoGameLoop( void ) {
	resettimeout();
	handleevents();
	getpackets();
	dnDrawLoadingScreen( *waitStatus ? waitStatus : NULL );
}

static
int dnPlayerIndex( steam_id_t steamID ) {
	int result = -1;
	
	for ( int i = 0; i < numPlayers; i++ ) {
		if ( playerIDs[i] == steamID ) {
			result = i;
		}
	}
	
	return result;
}

static
void dnApplyPlayerPrefs( ) {
#if 0
	for ( int userIndex = 0; userIndex < numPlayers; userIndex++ ) {
		for ( int i = 0; i < MAX_WEAPONS; i++ ) {
			ud.wchoice[userIndex][i] = playerPrefs[userIndex].wchoice[i];
		}
		ps[userIndex].auto_aim = playerPrefs[userIndex].autoAim;
		ps[userIndex].aim_mode = playerPrefs[userIndex].aimMode;
		ps[userIndex].weaponswitch = playerPrefs[userIndex].weaponswitch;
	}
#else
	Sys_DPrintf( "[DUKEMP] dnApplyPlayerPrefs: player prefs crc32 = %u\n", crc32once( (unsigned char*)&playerPrefs[0], sizeof( playerPrefs ) ) );
#endif
}

static
void dnBeginBlockDownload( void ) {
	block.chunkMask = 0;
	block.size = 0;
	block.transmissionInProgress = 1;
}

static
int dnGetNumChunks( ) {
	int numChunks;
	
	if ( block.size % BLOCK_CHUNK_SIZE == 0 ) {
		numChunks = block.size / BLOCK_CHUNK_SIZE;
	} else {
		numChunks = block.size / BLOCK_CHUNK_SIZE + 1;
	}
	
	return numChunks;
}

static
void dnBeginBlockUpload( const void *data, unsigned int size ) {
	if ( size <= sizeof( block.buffer ) ) {
		memcpy( (void*)&block.buffer[0], data, size );
		block.size = size;
		block.transmissionInProgress = 1;
		block.chunkMask = 0;
		for ( int i = 0, numChunks = dnGetNumChunks(); i < numChunks; i++ ) {
			block.chunkMask |= ( 1 << i );
		}
	} else {
		Sys_DPrintf( "[DUKEMP] dnBeginBlockUpload: data is too big\n" );
	}
}

static
int dnIsBlockTransmissionFinished( void ) {
	return !block.transmissionInProgress;
}

static
void dnGetChunkBounds( unsigned int chunkIndex, unsigned int *offset, unsigned int *size ) {
	int numChunks = dnGetNumChunks();
	
	if ( chunkIndex < numChunks ) {
		*offset = chunkIndex * BLOCK_CHUNK_SIZE;
		if ( chunkIndex + 1 == dnGetNumChunks() ) {
			*size = block.size - *offset;
		} else {
			*size = BLOCK_CHUNK_SIZE;
		}
	} else {
		Sys_DPrintf( "[DUKEMP] dnGetChunkBounds: wrong chunk index\n" );
	}
	
}

static
void dnSendNextChunk( int playerIndex ) {
	blockDownloadStatus_t *bds = &clientBlockStatus[playerIndex];
	int numChunks = dnGetNumChunks();
	blockChunk_t *blockChunk;
	unsigned int offset, size;
	int chunksSent;
	
	chunksSent = 0;
	for ( int i = 0; i < numChunks; i++ ) {
		if ( ( bds->chunkMask & ( 1 << i ) ) == 0 ) {
			Sys_DPrintf( "[DUKEMP] dnSendNextChunk: sending chunk %d of %d\n", i + 1, numChunks );
			
			dnGetChunkBounds( i, &offset, &size );
			blockChunk = (blockChunk_t*)malloc( sizeof( blockChunk_t ) + size );
			blockChunk->header.sessionToken = sessionToken;
			blockChunk->header.tag = TAG_BLOCK_CHUNK;
			blockChunk->chunkIndex = i;
			blockChunk->allBlockChunks = block.chunkMask;
			blockChunk->chunkMask = bds->chunkMask | ( 1 << i );
			blockChunk->chunkOffset = offset;
			blockChunk->chunkSize = size;
			blockChunk->blockSize = block.size;
			memcpy( (void*)&blockChunk->chunkData[0], (void*)&block.buffer[offset], size );
			chunksSent += CSTEAM_SendPacket( playerIDs[playerIndex], (void*)blockChunk, sizeof( blockChunk_t ) + size, 1, CHAN_SYNC );
			
			Sys_DPrintf( "[DUKEMP] crc32 = %u\n", crc32once( &blockChunk->chunkData[0], blockChunk->chunkSize ) );
		}
		if ( chunksSent >= CHUNKS_AT_TIME ) {
			break;
		}
	}
	
	if ( chunksSent == 0 ) {
		Sys_DPrintf( "[DUKEMP] dnSendNextChunk: all chunks sent\n" );
		
		blockChunk = (blockChunk_t*)malloc( sizeof( blockChunk_t ) );
		blockChunk->header.sessionToken = sessionToken;
		blockChunk->header.tag = TAG_BLOCK_CHUNK;
		blockChunk->chunkIndex = 0xFFFFFFFF;
		blockChunk->allBlockChunks = block.chunkMask;
		blockChunk->chunkMask = bds->chunkMask;
		blockChunk->chunkOffset = 0;
		blockChunk->chunkSize = 0;
		blockChunk->blockSize = block.size;
		
		CSTEAM_SendPacket( playerIDs[playerIndex], (void*)blockChunk, sizeof( blockChunk_t ), 1, CHAN_SYNC );
	}
}

static
void dnReceiveBlock( void ) {
	double lastStatusSent;
	double now;
	blockDownloadStatus_t bds;
	
	Sys_DPrintf( "[DUKEMP] dnReceiveBlock: starting block downloading\n" );
	
	bds.header.sessionToken = sessionToken;
	bds.header.tag = TAG_BLOCK_STATUS;
	bds.chunkMask = block.chunkMask;
	
	lastStatusSent = -999999.0;
	
	do {
		dnDoGameLoop();

		now = Sys_GetTicks();
		
		if ( lastStatusSent + BLOCK_REQUEST_DELAY < now ) {
			bds.chunkMask = block.chunkMask;
			Sys_DPrintf( "[DUKEMP] dnReceiveBlock: request with mask %d\n", bds.chunkMask );
			CSTEAM_SendPacket( playerIDs[0], (void*)&bds, sizeof( blockDownloadStatus_t ), 1, CHAN_SYNC );
			lastStatusSent = Sys_GetTicks();
		}
		
	} while ( !dnIsBlockTransmissionFinished() );
	
	Sys_DPrintf( "[DUKEMP] dnReceiveBlock: block successfully downloaded\n" );
}

static
void dnFillPrematchStatusPacket( prematchStatusPacket_t *psp ) {
	memset( psp, 0, sizeof( prematchStatusPacket_t ) );
	psp->header.sessionToken = sessionToken;
	psp->header.tag = TAG_PREMATCH;
	psp->timeToStart = -1;
	psp->readyPlayers = readyPlayers;
	memcpy( &psp->playerPrefs[0], &playerPrefs[0], sizeof( playerPrefs_t ) * MAXPLAYERS );
}

static
void dnWaitForClients( blockKind_t blockKind ) {
	double waitStart;
	double lastUpdateSent;
	double startTime;
	double now;
	double allReadyTime;
	unsigned char allReady;
	double delaySum[MAXPLAYERS] = { 0 };
	int numUpdates[MAXPLAYERS] = { 0 };

	prematchStatusPacket_t psp;
	
	if ( blockKind == BLOCK_SNAPSHOT ) {
		compressedSnapshot_t *cs = dnCompressSnapshot( &snapshot );
		Sys_DPrintf( "[DUKEMP] dnWaitForClients: snapshot taken, crc32 = %u\n", crc32once( &cs->data[0], cs->size ) );
		dnBeginBlockUpload( cs->data, cs->size );
		free( (void*)cs );
	}
	
	allReady = 0;
	dnIterPlayers( i ) {
		if ( i != 0 ) {
			allReady |= 1 << i;
		}
	}
	
	readyPlayers = 0;
	lastUpdateSent = -999999.0; /* to trigger sending within the loop */
	
	wfeState = STATE_PREMATCH;
	
	Sys_DPrintf( "[DUKEMP] dnWaitForClients is starting\n");
	
	waitStart = Sys_GetTicks();
	
	do {
		
		sprintf( waitStatus, "Waiting for players: %d of %d", dnNumBitsSet( readyPlayers ), numPlayers - 1 );
		
		dnDoGameLoop();
		
		now = Sys_GetTicks();
		
		if ( lastUpdateSent + PREMATCH_STATUS_RESEND_DELAY < now ) {
			Sys_DPrintf( "[DUKEMP] dnWaitForClients: resending status\n" );
			
			dnFillPrematchStatusPacket( &psp );
			
			dnIterPlayers( i ) {
				CSTEAM_SendPacket( playerIDs[i], (void*)&psp, sizeof( prematchStatusPacket_t ), 1, CHAN_SYNC );
				delaySum[i] += Sys_GetTicks() - now;
				numUpdates[i] ++;
			}
			lastUpdateSent = Sys_GetTicks();
		}
	} while ( allReady != readyPlayers && waitStart + WFE_DELAY > now );
	
	
	if ( allReady == readyPlayers ) {
		
		wfeState = STATE_AWAITING_MATCH;
		
		sprintf( waitStatus, "All ready" );
		
		Sys_DPrintf( "[DUKEMP] dnWaitForClients: all ready\n" );
		
		now = Sys_GetTicks();
		allReadyTime = now - waitStart;
		
		if ( allReadyTime > ALLCLEAR_DELAY ) {
			allReadyTime = ALLCLEAR_DELAY;
		}
		
		startTime = now + allReadyTime;
		
		dnFillPrematchStatusPacket( &psp );
		
		dnIterPlayers( i ) {
			psp.timeToStart = startTime - Sys_GetTicks() - ( delaySum[i] / numUpdates[i] ) ;
			if ( psp.timeToStart < 0 ) {
				psp.timeToStart = 0;
			}
			CSTEAM_SendPacket( playerIDs[i], (void*)&psp, sizeof( prematchStatusPacket_t ), 1, CHAN_SYNC );
			dnDoGameLoop();
		}
		
		if ( blockKind == BLOCK_SNAPSHOT ) {
			dnLoadSnapshot( &snapshot );
			Sys_DPrintf( "[DUKEMP] dnWaitForClients: snapshot restored\n" );
		}
		
		dnApplyPlayerPrefs();		
		clearfifo();
		
		Sys_DPrintf( "[DUKEMP] dnWaitForClients: all clear sent, waiting the rest\n" );
		do {
			sprintf( waitStatus, "Starting in %d...", (int)( 0.5 + ( startTime - Sys_GetTicks() ) / 1000.0 ) );
			dnDoGameLoop();
		} while ( startTime > Sys_GetTicks() );
		
	} else {
		Sys_DPrintf( "[DUKEMP] dnWaitForClients: not all clients are ready\n" );
		gameexit( "Players are not ready\n" );
	}
	wfeState = STATE_INACTIVE;
}

static
void dnSendReady( void ) {
	readyPacket_t readyPacket;
	readyPacket.header.sessionToken = sessionToken;
	readyPacket.header.tag = TAG_READY;
	readyPacket.playerPrefs.aimMode = ud.mouseaiming;
	readyPacket.playerPrefs.autoAim = AutoAim;
	readyPacket.playerPrefs.weaponswitch = ud.weaponswitch;
	for ( int i = 0; i < MAX_WEAPONS; i++ ) {
		readyPacket.playerPrefs.wchoice[i] = ud.wchoice[myConnectIndex][i] & 0xFF;
	}
	CSTEAM_SendPacket( playerIDs[0], &readyPacket, sizeof( readyPacket_t ), 1, CHAN_SYNC );
}

static
void dnWaitForServer( blockKind_t blockKind ) {
	double waitStart;
	double lastReadySent;
	double now;
	int iAmReady = 0;
	
	if ( blockKind != BLOCK_NONE ) {
		dnBeginBlockDownload();
		
		wfeState = STATE_DOWNLOADING;
		
		dnReceiveBlock();
		Sys_DPrintf( "[DUKEMP] dnWaitForServer: block download finished, crc32 = %u\n", crc32once( &block.buffer[0], block.size ) );
		if ( blockKind == BLOCK_SNAPSHOT ) {
			dnDecompressSnapshotRAW( (void*)&block.buffer[0], block.size, &snapshot );
		}
	}
	
	timeToStart = -1;
	readyPlayers = 0;
	lastReadySent = -999999.0; /* to trigger sending within the loop */
	
	wfeState = STATE_PREMATCH;
	
	Sys_DPrintf( "[DUKEMP] dnWaitForServer is starting\n" );
	
	waitStart = Sys_GetTicks();
	
	do {
		if ( !iAmReady ) {
			sprintf( waitStatus, "Waiting for server response...\n" );
		} else {
			sprintf( waitStatus, "Waiting for players: %d of %d", dnNumBitsSet( readyPlayers ), numPlayers - 1 );
		}

		dnDoGameLoop();
		
		now = Sys_GetTicks();
		
		iAmReady = ( ( readyPlayers & ( 1 << myConnectIndex ) ) != 0 );
		
		if ( !iAmReady ) {
			if ( lastReadySent + READY_RESEND_DELAY < now ) {
				Sys_DPrintf( "[DUKEMP] dnWaitForServer: resending ready\n" );
				dnSendReady();
				lastReadySent = now;
			}
		}
		
	} while ( timeToStart < 0 && waitStart + WFE_DELAY > now );
	
	if ( timeToStart < 0 ) {
		wfeState = STATE_INACTIVE;
		
		Sys_DPrintf( "[DUKEMP] dnWaitForServer: server is not responding\n" );
		gameexit( "Server is not responding" );
	} else {
		wfeState = STATE_AWAITING_MATCH;
		
		Sys_DPrintf( "[DUKEMP] dnWaitForServer: waiting for game, starting in %g sec\n", ( timeToStart / 1000.0 ) );
		if ( blockKind == BLOCK_SNAPSHOT ) {
			dnLoadSnapshot( &snapshot );
			Sys_DPrintf( "[DUKEMP] dnWaitForServer: snapshot restored\n" );
		}
		
		dnApplyPlayerPrefs();
		clearfifo();

		waitStart = Sys_GetTicks();
		do {
			sprintf( waitStatus, "Starting in %d...", (int)( ( 0.5 + waitStart + timeToStart - Sys_GetTicks() ) / 1000.0 ) );
			dnDoGameLoop();
		} while ( waitStart + timeToStart > Sys_GetTicks() );
		Sys_DPrintf( "[DUKEMP] dnWaitForServer: waiting done\n" );
	}
	
	wfeState = STATE_INACTIVE;
}

static
void dnProcessNotification( int senderIndex, notificationPacket_t *notificationPacket ) {
	switch ( notificationPacket->notification ) {
		case NOTIFICATION_OUT_OF_SYNC: {
			Sys_DPrintf( "[DUKEMP] dnProcessNotification: got out-of-sync from %d\n", senderIndex );
			if ( !awaitingResync ) {
				dnTakeSnapshot( &snapshot );
				awaitingResync = true;
				dnIterPlayers( i ) {
					dnNotifyPlayer( i, NOTIFICATION_FORCE_SYNC );
				}
			}
			break;
		}
			
		case NOTIFICATION_FORCE_SYNC: {
			if ( senderIndex == 0 ) {
				Sys_DPrintf( "[DUKEMP] dnProcessNotification: performing force resync\n" );
				doLoadSnapshot = true;
			} else {
				Sys_DPrintf( "[DUKEMP] dnProcessNotification: forcesync command from non-server\n" );
			}
			break;
		}
			
		default: {
			Sys_DPrintf( "[DUKEMP] dnProcessNotification: unknown notification code: %d\n", notificationPacket->notification );
			break;
		}
	}
}

static
void dnProcessPacket( steam_id_t sender, const void *packetData, unsigned int msgSize ) {
	packetHeader_t *packetHeader = (packetHeader_t*)packetData;
	pingPacket_t *pingPacket = (pingPacket_t*)packetData;
	readyPacket_t *readyPacket = (readyPacket_t*)packetData;
	prematchStatusPacket_t *prematchStatusPacket = (prematchStatusPacket_t*)packetData;
	blockDownloadStatus_t *blockDownloadStatus = (blockDownloadStatus_t*)packetData;
	blockChunk_t *blockChunk = (blockChunk_t*)packetData;
	notificationPacket_t *notificationPacket = (notificationPacket_t*)packetData;
	quitPacket_t *quitPacket = (quitPacket_t*)packetData;
	
	switch ( packetHeader->tag ) {
		case TAG_PING: {
			Sys_DPrintf( "[DUKEMP] dnProcessPacket: ping request from %s\n", CSTEAM_FormatId( sender ) );
			CSTEAM_SendPacket( sender, packetData, msgSize, 0, CHAN_PONG );
			break;
		}
		case TAG_READY: {
			Sys_DPrintf( "[DUKEMP] dnProcessPacket: got ready packet from %s\n", CSTEAM_FormatId( sender ) );
			int playerIndex = dnPlayerIndex( sender );
			if ( playerIndex > 0 ) {
				memcpy( &playerPrefs[playerIndex], &readyPacket->playerPrefs, sizeof( playerPrefs_t ) );
				readyPlayers |= ( 1 << playerIndex );
			} else {
				Sys_DPrintf( "[DUKEMP] dnProcessPacket: invalid user index\n" );
			}
			break;
		}
		case TAG_PREMATCH: {
			Sys_DPrintf( "[DUKEMP] dnProcessPacket: got prematch status packet from %s\n", CSTEAM_FormatId(( sender ) ) );
			if ( wfeState == STATE_PREMATCH ) {
				int playerIndex = dnPlayerIndex( sender );
				if ( playerIndex == 0 ) {
					memcpy( &playerPrefs[0], &prematchStatusPacket->playerPrefs[0], sizeof( playerPrefs_t ) * MAXPLAYERS );
					readyPlayers = prematchStatusPacket->readyPlayers;
					timeToStart = prematchStatusPacket->timeToStart;
				} else {
					Sys_DPrintf( "[DUKEMP] dnProcessPacket: got prematch packet from non-server\n" );
				}
			} else {
				Sys_DPrintf( "[DUKEMP] dnProcessPacket: not in PREMATCH state\n" );
			}
			break;
		}
		case TAG_BLOCK_STATUS: {
			Sys_DPrintf( "[DUKEMP] dnProcessPacket: got block status from %s\n", CSTEAM_FormatId( sender ) );
			int playerIndex = dnPlayerIndex( sender );
			if  ( playerIndex > 0 ) {
				if ( block.transmissionInProgress ) {
					memcpy( &clientBlockStatus[playerIndex], blockDownloadStatus, sizeof( blockDownloadStatus_t ) );
					dnSendNextChunk( playerIndex );
				} else {
					Sys_DPrintf( "[DUKEMP] dnProcessPacket: no transmission in progress\n" );
				}
			} else {
				Sys_DPrintf( "[DUKEMP] dnProcessPacket: got block status from unknown user\n" );
			}
			break;
		}
		case TAG_BLOCK_CHUNK: {
			Sys_DPrintf( "[DUKEMP] dnProcessPacket: got block chunk from %s\n", CSTEAM_FormatId( sender ) );
			int playerIndex = dnPlayerIndex( sender );
			if ( playerIndex == 0 ) {
				if ( blockChunk->chunkIndex != 0xFFFFFFFF ) {
					Sys_DPrintf( "[DUKEMP] dnProcessPacket: merging chunk %d\n", blockChunk->chunkIndex + 1 );
					memcpy( (void*)&block.buffer[blockChunk->chunkOffset], (void*)&blockChunk->chunkData[0], blockChunk->chunkSize );	
					block.chunkMask |= ( 1 << blockChunk->chunkIndex );
					block.size = blockChunk->blockSize;
					Sys_DPrintf( "[DUKEMP] crc32 = %u\n", crc32once( &blockChunk->chunkData[0], blockChunk->chunkSize ) );
					if ( block.chunkMask == blockChunk->allBlockChunks ) {
						Sys_DPrintf( "[DUKEMP] dnProcessPacket: all chunks downloaded\n" );
						block.transmissionInProgress = 0;
					}
				} else {
					Sys_DPrintf( "[DUKEMP] dnProcessPacket: transmission is over\n" );
					block.transmissionInProgress = 0;
				}
			} else {
				Sys_DPrintf( "[DUKEMP] dnProcessPacket: got block chunk packet from non-server\n" );
			}
			break;
		}
		case TAG_NOTIFICATION: {
			Sys_DPrintf( "[DUKEMP] dnProcessPacket: got notification from %s\n", CSTEAM_FormatId( sender ) );
			int playerIndex = dnPlayerIndex( sender );
			if ( dnIsPlayerIndexValid( playerIndex ) ) {
				dnProcessNotification( playerIndex, notificationPacket );
			} else {
				Sys_DPrintf( "[DUKEMP] dnProcessPacket: notification from unknown client\n" );
			}
			break;
		}
		case TAG_QUIT: {
			Sys_DPrintf( "[DUKEMP] dnProcessPacket: got quit packet from %s\n", CSTEAM_FormatId( sender ) );
			int playerIndex = dnPlayerIndex( sender );
			if ( dnIsPlayerIndexValid( playerIndex ) ) {
				
			} else {
				Sys_DPrintf( "[DUKEMP] dnProcessPacket: got quit packet from unknow client\n" );
			}
			break;
		}
		default: {
			Sys_DPrintf( "[DUKEMP] got packet with unknown tag %d from %s\n", packetHeader->tag, CSTEAM_FormatId( sender ) );
		}
	}
}

extern "C"
void dnGetPackets( void ) {
	unsigned int msgSize;
	steam_id_t sender;
	packetHeader_t *packetHeader;
	
	while ( CSTEAM_IsPacketAvailable( &msgSize, CHAN_SYNC ) ) {
		if ( CSTEAM_ReadPacket( (void*)packetBuffer, MAX_PACKET_SIZE, &msgSize, &sender, CHAN_SYNC ) ) {
			packetHeader = (packetHeader_t*)&packetBuffer[0];
			if ( packetHeader->sessionToken == sessionToken || packetHeader->sessionToken == 0xDEADBEEF ) {
				dnProcessPacket( sender, (void*)&packetBuffer[0], msgSize );
			} else {
				Sys_DPrintf( "[DUKEMP] dnGetPackets: got packet with incorrect session token from %s\n", CSTEAM_FormatId( sender ) );
			}
		} else {
			Sys_DPrintf( "[DUKEMP] dnGetPackets: error reading packet\n" );
		}
	}
}

extern "C"
int dnPing( steam_id_t remote ) {
	pingPacket_t pingPacket;
	pingPacket.header.sessionToken = 0xDEADBEEF;
	pingPacket.header.tag = TAG_PING;
	pingPacket.value = rand();
	CSTEAM_SendPacket( remote, (void*)&pingPacket, sizeof( pingPacket_t ), 0, CHAN_SYNC );
	return pingPacket.value;
}

extern "C"
int  dnCheckPong( steam_id_t remote, int value ) {
	unsigned int msgSize;
	steam_id_t sender;
	pingPacket_t *pingPacket;
	int retval = 0;
	
	while ( CSTEAM_IsPacketAvailable( &msgSize, CHAN_PONG ) ) {
		if ( CSTEAM_ReadPacket( (void*)packetBuffer, MAX_PACKET_SIZE, &msgSize, &sender, CHAN_PONG ) ) {
			pingPacket = (pingPacket_t*)&packetBuffer[0];
			if ( pingPacket->header.tag == TAG_PING ) {
				Sys_DPrintf( "[DUKEMP] dnCheckPong: got pong from %s\n", CSTEAM_FormatId( sender ) );
				retval = 1;
				break;
			} else {
				Sys_DPrintf( "[DUKEMP] dnCheckPong: got junk over PONG channel from %s\n", CSTEAM_FormatId( sender ) );
			}
		} else {
			Sys_DPrintf( "[DUKEMP] dnCheckPong: error reading packet\n" );
		}
	}
	
	return retval;
}

extern "C"
int  dnEnterMultiMode( const lobby_info_t *lobbyInfo ) {
	int retval = 1;
	
	memset( &playerPrefs[0], 0, sizeof( playerPrefs ) );

	sessionToken = lobbyInfo->session_token;
	numPlayers = lobbyInfo->num_players;
	numActivePlayers = numPlayers;
	doLoadSnapshot = false;
	
	myConnectIndex = -1;
	for ( int i = 0; i < numPlayers; i++ ) {
		playerIDs[i] = lobbyInfo->players[i].id;
#if 0
		memset( &ud.user_name[i][0], 0, sizeof( ud.user_name[i] ) );
		strncpy( &ud.user_name[i][0], dnFilterUsername( lobbyInfo->players[i].name ), sizeof( ud.user_name[i] ) - 1 );
#endif
		if ( playerIDs[i] == CSTEAM_MyID() ) {
			myConnectIndex = i;
		}
	}
	if ( myConnectIndex == -1 ) {
		Sys_DPrintf( "[DUKEMP] dnEnterMultiMode: wrong playerIDs array\n" );
		retval = 0;
	}
	
	dnDisableTimeouts();
		
	return retval;
}

extern "C"
void dnExitMultiMode( void ) {
	dnDisableTimeouts();
	sessionToken = 0xDEADBEEF;
	numPlayers = 0;
	memset( &playerIDs[0], 0, sizeof( playerIDs ) );
}

extern "C"
int  dnIsHost( void ) {
	if ( numPlayers != 0 && myConnectIndex == 0 && numActivePlayers > 1 ) {
		return 1;
	}
	return 0;
}

extern "C"
int  dnIsInMultiMode( void ) {
	return numPlayers > 0 ? 1 : 0;
}

extern "C"
void dnRemovePlayer( int playerIndex ) {
	Sys_DPrintf( "[DUKEMP] dnRemovePlayer %d\n", playerIndex );
	
	playerIDs[playerIndex] = 0;
	dnUpdateActivePlayers();
}

extern "C"
void dnWaitForEverybody( blockKind_t blockKind ) {
	if ( dnIsInMultiMode() ) {
		dnDisableTimeouts();
		if ( dnIsHost() ) {
			dnWaitForClients( blockKind );
		} else {
			dnWaitForServer( blockKind );
		}
		dnEnableTimeouts();
	}
}

extern "C"
void dnNotifyPlayer( int playerIndex, notification_t notification ) {
	if ( dnIsPlayerIndexValid( playerIndex ) ) {
		notificationPacket_t np;
		np.header.sessionToken = sessionToken;
		np.header.tag = TAG_NOTIFICATION;
		np.notification = notification;
		CSTEAM_SendPacket( playerIDs[playerIndex], (void*)&np, sizeof( notificationPacket_t ), 1, CHAN_SYNC );
	}
}

extern "C"
int  dnSendPacket( int playerIndex, const void *bufptr, int size ) {
	int retval = 0;
	if ( dnIsPlayerIndexValid( playerIndex ) ) {
		retval = CSTEAM_SendPacket( playerIDs[playerIndex], bufptr, size, NET_RELIABLE, CHAN_LEGACY );
	}
	return retval;
}

extern "C"
int dnGetPacket( int *playerIndex, void *bufptr, int size ) {
	int result = 0;
	unsigned int bufsize = 0;
	
	*playerIndex = -1;
	if ( CSTEAM_IsPacketAvailable( &bufsize, CHAN_LEGACY ) ) {
		steam_id_t remote;
		unsigned int msgsize;
		if ( CSTEAM_ReadPacket( bufptr, size, &msgsize, &remote, CHAN_LEGACY ) ) {
			result = (int)msgsize;
			*playerIndex = dnPlayerIndex( remote );
		}
	}
	
	return result;
}

extern "C"
void dnResyncIfNeeded( void ) {
	if ( doLoadSnapshot ) {
		awaitingResync = false;
		doLoadSnapshot = false;
//		dnWaitForEverybody( BLOCK_SNAPSHOT );
	}
}

extern "C"
void dnDisconnect( void ) {
	dnSendQuit( 0, QUIT_LEFT );
}
