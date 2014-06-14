//
//  dnSnapshot.h
//  duke3d
//
//  Created by serge on 24/12/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#ifndef DNSNAPSHOT_H
#define DNSNAPSHOT_H

#ifdef __cplusplus
extern "C" {
#endif
	
#include "duke3d.h"
#include "build.h"

#define WITH_SCRIPTS 1
#define WITH_HITTYPE 1
	
#pragma pack(push,1)
	
typedef struct {
	short numwalls;
	walltype wall[MAXWALLS];
	short numsectors;
	sectortype sector[MAXSECTORS];
	spritetype sprite[MAXSPRITES];
	spriteexttype spriteext[MAXSPRITES];
	short headspritesect[MAXSECTORS+1];
	short prevspritesect[MAXSPRITES];
	short nextspritesect[MAXSPRITES];
	short headspritestat[MAXSTATUS+1];
	short prevspritestat[MAXSPRITES];
	short nextspritestat[MAXSPRITES];
	short numcyclers;
	short cyclers[MAXCYCLERS][6];
	struct player_struct ps[MAXPLAYERS];
	struct player_orig po[MAXPLAYERS];
	short numanimwalls;
	struct animwalltype animwall[MAXANIMWALLS];
	long msx[2048];
	long msy[2048];
	short spriteqloc;
	short spriteqamount;
	short spriteq[1024];
	short mirrorcnt;
	short mirrorwall[64];
	short mirrorsector[64];
	/* char show2dsector[(MAXSECTORS+7)>>3]; */
	char actortype[MAXTILES-VIRTUALTILES];
	short numclouds;
	short clouds[128];
	short cloudx[128];
	short cloudy[128];
	
#if WITH_SCRIPTS
	unsigned char scriptptrs[MAXSCRIPTSIZE];
	unsigned int script[MAXSCRIPTSIZE];
	unsigned int actorscrptr[MAXTILES-VIRTUALTILES];
#endif
	
#if WITH_HITTYPE
	unsigned char hittypeflags[MAXSPRITES];
	struct weaponhit hittype[MAXSPRITES];
#endif
	
	long lockclock;
	short pskybits;
	short pskyoff[MAXPSKYTILES];
	long animatecnt;
	short animatesect[MAXANIMATES];
	long animateptr[MAXANIMATES];
	long animategoal[MAXANIMATES];
	long animatevel[MAXANIMATES];
	char earthquaketime;
	short ud_from_bonus;
	short ud_secretlevel;
	int ud_respawn_monsters;
	int ud_respawn_items;
	int ud_respawn_inventory;
	int ud_monsters_off;
	int ud_coop;
	int ud_marker;
	int ud_ffire;
	char numplayersprites;
	short frags[MAXPLAYERS][MAXPLAYERS];
	long randomseed;
	short global_random;
	long parallaxyscale;
	char padding[8]; // must be zero
} snapshot_t;
	
typedef struct {
	unsigned int size;
	unsigned char data[1];
} compressedSnapshot_t;
	
typedef struct {
	char *bitmask;		// LZ4-compressed bitmask of changed bytes positions
	int bitmaskSize;
	char *stream;		// values of changed bytes in order of their appearance
	int streamSize;
} snapshotDelta_t;

#pragma pack(pop)

void dnTakeSnapshot( snapshot_t *snapshot );
void dnRestoreSnapshot( const snapshot_t *snapshot );
void dnLoadSnapshot( const snapshot_t *snapshot );

compressedSnapshot_t* dnCompressSnapshot( const snapshot_t *snapshot );
void dnDecompressSnapshotRAW( const void *data, unsigned int size, snapshot_t *snapshot );
void dnDecompressSnapshot( const compressedSnapshot_t *compressedSnapshot, snapshot_t *snapshot );
	
void dnCalcDelta( const snapshot_t *olds, const snapshot_t *news, snapshotDelta_t *delta );
	
void dnCompareSnapshots( const snapshot_t *olds, const snapshot_t *news );
		
#ifdef __cplusplus
}
#endif

#endif /* DNSNAPSHOT_H */
