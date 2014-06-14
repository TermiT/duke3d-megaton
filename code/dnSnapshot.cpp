//
//  dnSnapshot.cpp
//  duke3d
//
//  Created by serge on 24/12/13.
//  Copyright (c) 2013 General Arcade. All rights reserved.
//

#include <vector>
#include <sstream>
#include <string>
#include <stdlib.h>
#include "dnSnapshot.h"
#include "dnAPI.h"

extern "C" {
#include "lz4.h"
}

#define copyval(x) snapshot->x = x;
#define copyarr(x) memcpy( &snapshot->x[0], &x[0], sizeof( snapshot->x ) );
#define copyudval(x) snapshot->ud_##x = ud.x;
#define copyudval_m(x) snapshot->ud_##x = ud.x;
#define copyoffs(x,base) \
	for (int i = sizeof(x)/sizeof(x[0]); i>=0; i--) \
		snapshot->x[i] = (long)x[i]-(long)base;

extern "C"
void dnTakeSnapshot( snapshot_t *snapshot ) {	
	copyval( numwalls );
	copyarr( wall );
	copyval( numsectors );
	copyarr( sector );
	copyarr( sprite );
	copyarr( spriteext );
	copyarr( headspritesect );
	copyarr( prevspritesect );
	copyarr( nextspritesect );
	copyarr( headspritestat );
	copyarr( prevspritestat );
	copyarr( nextspritestat );
	copyval( numcyclers );
	copyarr( cyclers );
	copyarr( ps );
	copyarr( po );
	copyval( numanimwalls );
	copyarr( animwall );
	copyarr( msx );
	copyarr( msy );
	copyval( spriteqloc );
	copyval( spriteqamount );
	copyarr( spriteq );
	copyval( mirrorcnt );
	copyarr( mirrorwall );
	copyarr( mirrorsector );
	/* char show2dsector[(MAXSECTORS+7)>>3]; */
	copyarr( actortype );
	
	copyval( numclouds );
	copyarr( clouds );
	copyarr( cloudx );
	copyarr( cloudy );
		
#if WITH_SCRIPTS
	for (int i = 0; i < MAXSCRIPTSIZE; i++ ) {
		if ( (long)script[i] >= (long)(&script[0]) && (long)script[i] < (long)(&script[MAXSCRIPTSIZE]) ) {
			snapshot->scriptptrs[i] = 1;
			snapshot->script[i] = (long)script[i] - (long)&script[0];
		}
		else {
			snapshot->scriptptrs[i] = 0;
			snapshot->script[i] = 0xFFFFFFFF;
		}
	}
	
	for( int i = 0; i < MAXTILES-VIRTUALTILES; i++ ) {
		if ( actorscrptr[i] ) {
			snapshot->actorscrptr[i] = (long)actorscrptr[i]-(long)&script[0];
		} else {
			snapshot->actorscrptr[i] = 0;
		}
	}
#endif
	
#if WITH_HITTYPE
	for( int i = 0; i < MAXSPRITES; i++ ) {
		weaponhit wh = { 0 };
        snapshot->hittypeflags[i] = 0;

		memcpy( &wh, &hittype[i], sizeof( weaponhit ) );

        if ( actorscrptr[ sprite[i].picnum ] != 0 ) {
			unsigned int begin = (unsigned int)&script[0];
			unsigned int end = (unsigned int)&script[MAXSCRIPTSIZE];
			
			if ( hittype[i].temp_data[1] >= begin &&
				hittype[i].temp_data[1] < end ) {
				snapshot->hittypeflags[i] |= 1;
				wh.temp_data[1] = hittype[i].temp_data[1] - begin;
			}
			if ( hittype[i].temp_data[4] >= begin &&
				hittype[i].temp_data[4] < end ) {
				snapshot->hittypeflags[i] |= 2;
				wh.temp_data[4] = hittype[i].temp_data[4] - begin;
			}
			if ( hittype[i].temp_data[5] >= begin &&
				hittype[i].temp_data[5] < end ) {
				snapshot->hittypeflags[i] |= 4;
				wh.temp_data[5] = hittype[i].temp_data[5] - begin;
			}
		}
		memcpy( &snapshot->hittype[i], &wh, sizeof( weaponhit ) );
    }
#endif
	
	copyval( lockclock );
	copyval( pskybits );
	copyarr( pskyoff );
	copyval( animatecnt );
	copyarr( animatesect );
	copyoffs( animateptr, &sector[0] );
	copyarr( animategoal );
	copyarr( animatevel );	
	copyval( earthquaketime );
	copyudval( from_bonus );
	copyudval( secretlevel );
	copyudval_m( respawn_monsters );
	copyudval_m( respawn_items );
	copyudval_m( respawn_inventory );
	copyudval_m( monsters_off );
	copyudval_m( coop );
	copyudval_m( marker );
	copyudval_m( ffire );
	copyval( numplayersprites );
	copyarr( frags );
	copyval( randomseed );
	copyval( global_random );
	copyval( parallaxyscale );

	memset( &snapshot->padding[0], 0, sizeof( snapshot->padding ) );
}

#undef copyval
#undef copyarr
#undef copyudval
#undef copyudval_m
#undef copyoffs

#define copyval(x) x = snapshot->x;
#define copyarr(x) memcpy( &x[0], &snapshot->x[0], sizeof( snapshot->x ) );
#define copyudval(x) ud.x = snapshot->ud_##x;
#define copyudval_m(x) ud.x = ud.m_##x = snapshot->ud_##x;
#define copyoffs(x,base) \
for (int i = sizeof(x)/sizeof(x[0]); i>=0; i--) \
	x[i] = (long*)( ((long)base) + snapshot->x[i] )

extern "C"
void dnRestoreSnapshot( const snapshot_t *snapshot ) {
	copyval( numwalls );
	copyarr( wall );
	copyval( numsectors );
	copyarr( sector );
	copyarr( sprite );
	copyarr( spriteext );
	copyarr( headspritesect );
	copyarr( prevspritesect );
	copyarr( nextspritesect );
	copyarr( headspritestat );
	copyarr( prevspritestat );
	copyarr( nextspritestat );
	copyval( numcyclers );
	copyarr( cyclers );
	
	char *palette[MAXPLAYERS];
	char gm[MAXPLAYERS];
	
	for ( int i = 0; i < MAXPLAYERS; i++ ) {
		palette[i] = ps[i].palette;
		gm[i] = ps[i].gm;
	}
	copyarr( ps );
	for ( int i = 0; i < MAXPLAYERS; i++ ) {
		ps[i].palette = palette[i];
		ps[i].gm = gm[i];
	}
	
	copyarr( po );
	copyval( numanimwalls );
	copyarr( animwall );
	copyarr( msx );
	copyarr( msy );
	copyval( spriteqloc );
	copyval( spriteqamount );
	copyarr( spriteq );
	copyval( mirrorcnt );
	/* char show2dsector[(MAXSECTORS+7)>>3]; */
	copyarr( mirrorwall );
	copyarr( mirrorsector );
	copyarr( actortype );
	copyval( numclouds );
	copyarr( clouds );
	copyarr( cloudx );
	copyarr( cloudy );
	
#if WITH_SCRIPTS
	for ( int i = 0; i < MAXSCRIPTSIZE; i++ ) {
        if ( snapshot->scriptptrs[i] ) {
			script[i] = (long)&script[0] + snapshot->script[i];
		}
	}
	
	for( int i = 0; i < MAXTILES-VIRTUALTILES; i++ ) {
		if ( snapshot->actorscrptr[i] ) {
			actorscrptr[i] = (long*)( (long)(&script[0]) + snapshot->actorscrptr[i] );
		} else {
			actorscrptr[i] = 0;
		}
	}
#endif

#if WITH_HITTYPE
	copyarr( hittype );

	for ( int i = 0; i < MAXSPRITES; i++ ) {
		unsigned char flags = snapshot->hittypeflags[i];
		long j = (long)&script[0];
		if ( flags & 1 ) {
			T2 += j;
		}
		if ( flags & 2 ) {
			T5 += j;
		}
		if ( flags & 4 ) {
			T6 += j;
		}
	}
#endif
	
	copyval( lockclock );
	copyval( pskybits );
	copyarr( pskyoff );
	copyval( animatecnt );
	copyarr( animatesect );
	copyoffs( animateptr, &sector[0] ); /* !!! */
	copyarr( animategoal );
	copyarr( animatevel );
	copyval( earthquaketime );
	copyudval( from_bonus );
	copyudval( secretlevel );
	copyudval_m( respawn_monsters );
	copyudval_m( respawn_items );
	copyudval_m( respawn_inventory );
	copyudval_m( monsters_off );
	copyudval_m( coop );
	copyudval_m( marker );
	copyudval_m( ffire );
	copyval( numplayersprites );
	copyarr( frags );
	copyval( randomseed );
	copyval( global_random );
	copyval( parallaxyscale );
}

extern "C"
void dnLoadSnapshot( const snapshot_t *snapshot ) {
	Sys_DPrintf( "[DUKEMP] dnLoadSnapshot: loading snapshot\n" );	
	
	dnRestoreSnapshot( snapshot );
	resetinterpolations();
	show_shareware = 0;
	everyothertime = 0;
	resetmys();
	ready2send = 1;
	
//	waitforeverybody();
	
	resettimevars();
}

extern "C"
compressedSnapshot_t* dnCompressSnapshot( const snapshot_t *snapshot ) {
	compressedSnapshot_t *result;
	char *buffer = (char*)malloc( LZ4_compressBound( sizeof( snapshot_t ) ) );
	int size = LZ4_compress( (char*)snapshot, buffer, sizeof( snapshot_t ) );
	result = (compressedSnapshot_t*)malloc( sizeof( result->size ) + size );
	result->size = size;
	memcpy( &result->data[0], buffer, size );
	free( buffer );
	return result;
}

void dnDecompressSnapshotRAW( const void *data, unsigned int size, snapshot_t *snapshot ) {
	LZ4_decompress_safe( (char*)data, (char*)snapshot, size, sizeof( snapshot_t ) );
}

void dnDecompressSnapshot( const compressedSnapshot_t *compressedSnapshot, snapshot_t *snapshot ) {
	dnDecompressSnapshotRAW( (void*)&compressedSnapshot->data[0], compressedSnapshot->size, snapshot );
}

static
int rle_encode(const char *in, char *out, int l)
{
	int dl, i;
	char cp, c;
	
	for(cp=c= *in++, i = 0, dl=0; l>0 ; c = *in++, l-- ) {
		if ( c == cp ) {
			i++;
			if ( i > 255 ) {
				*out++ = 255;
				*out++ = c; dl += 2;
				i = 1;
			}
		} else {
			*out++ = i;
			*out++ = cp; dl += 2;
			i = 1;
		}
		cp = c;
	}
	*out++ = i; *out++ = cp; dl += 2;
	return dl;
}

void dnCalcDelta( const snapshot_t *olds, const snapshot_t *news, snapshotDelta_t *delta ) {
	unsigned char bitmask[ sizeof( snapshot_t ) / 8 ] = { 0 };
	std::vector<char> stream;
	char *oldp = (char*)olds;
	char *newp = (char*)news;
	
	stream.reserve( 256 * 1024 );
	for ( int i = 0; i < sizeof( snapshot_t ); i++, oldp++, newp++ ) {
		if ( *newp != *oldp ) {
			bitmask[ i/8 ] |= ( 1 << i%8 ); /* mark changed byte */
			stream.push_back( *newp ); /* put new value to the stream */
		}
	}
	
	std::vector<char> buffer;
	buffer.resize( LZ4_compressBound( sizeof( bitmask ) ) );
	
	delta->bitmaskSize = LZ4_compress( (char*)&bitmask[0], &buffer[0], sizeof( bitmask ));
	delta->bitmask = (char*)malloc( delta->bitmaskSize );
	memcpy( delta->bitmask, &buffer[0], delta->bitmaskSize );
	
	delta->streamSize = stream.size();
	delta->stream = (char*)malloc( delta->streamSize );
	memcpy( delta->stream, &stream[0], delta->streamSize );	
}

void dnApplyDelta( const snapshot_t *olds, snapshot_t *news, const snapshotDelta_t *delta ) {
	const char *streamPtr;
	const char *oldp = (const char*)olds;
	char *newp = (char*)news;
	unsigned char *bitmask;
	
	bitmask = (unsigned char*)malloc( sizeof( snapshot_t ) / 8 );
	LZ4_decompress_fast( delta->bitmask, (char*)bitmask, sizeof( snapshot_t ) / 8 );
	
	streamPtr = &delta->stream[0];
	
	for ( int i = 0; i < sizeof( snapshot_t ); i++, oldp++, newp++ ) {
		if ( bitmask[ i/8 ] & ( 1 << i%8 )) {
			*newp = *streamPtr;
			streamPtr++;
		} else {
			*newp = *oldp;
		}
	}
	
	free( bitmask );
	
	printf( "stream size: %d\n", delta->streamSize );
	printf( "read %d bytes\n", streamPtr - &delta->stream[0] );
}

static int64 sum = 0;
static int64 num = 0;
static int maxd = 0;

extern "C"
void dnCompareSnapshots( const snapshot_t *olds, const snapshot_t *news ) {
	char *p, *q;
	p = (char*)olds;
	q = (char*)news;
	int diff = 0;
	
	std::stringstream ss( std::ios::in | std::ios::out | std::ios::binary );
	
	char mask[sizeof(snapshot_t)];
	
	for (int i = 0; i < sizeof( snapshot_t ); i++, p++, q++ ) {
		if ( *q != *p ) {
			diff++;
			mask[i] = 1;
			ss.write(p, 1);
		} else {
			mask[i] = 0;
		}
	}

#if 0
	char *mask_lz4 = (char*)malloc( LZ4_compressBound( sizeof(mask) ) );
	int mask_lz4_size = LZ4_compress( (char*)&mask[0], (char*)mask_lz4, sizeof(mask));
	free( mask_lz4 );
#endif
	unsigned char bitmask[sizeof(mask)/8];
	for ( int i = 0; i < sizeof( mask )/8; i++ ) {
		bitmask[i] =	mask[ i*8 + 0 ] << 0 &
						mask[ i*8 + 1 ] << 1 &
						mask[ i*8 + 2 ] << 2 &
						mask[ i*8 + 3 ] << 3 &
						mask[ i*8 + 4 ] << 4 &
						mask[ i*8 + 5 ] << 5 &
						mask[ i*8 + 6 ] << 6 &
						mask[ i*8 + 7 ] << 7;
	}
	
	char *bitmask_lz4 = (char*)malloc( LZ4_compressBound( sizeof(bitmask) ) );
	int bitmask_lz4_size = LZ4_compress((char*)&bitmask[0], (char*)bitmask_lz4, sizeof( bitmask ));
	free( bitmask_lz4 );

#if 0
	char *mask_rle = (char*)malloc( sizeof(mask)*2 );
	int mask_rle_size = rle_encode((char*)&mask[0], (char*)mask_rle, sizeof(mask));
	free( mask_rle );
		
	std::string dfs = ss.str();
	char *payload_lz4 = (char*)malloc( LZ4_compressBound( dfs.size() ) );
	int payload_lz4_size = LZ4_compress( dfs.c_str(), payload_lz4, dfs.size() );
	free( payload_lz4 );
	
#endif

#if 0
	printf( "*** DATA INFO ***\n" );
	printf( "%d bytes differ\n", diff );
	printf( "mask size: %d\n", sizeof( mask ));
	printf( "bitmask size: %d\n", sizeof( bitmask ) );
	printf( "mask lz4 size: %d\n", mask_lz4_size );
	printf( "bitmask lz4 size: %d\n", bitmask_lz4_size );
	printf( "mask rle size: %d\n", mask_rle_size );
	
	printf( "diff stream size: %d\n", dfs.size() );
	printf( "diff stream lz4 size: %d\n", payload_lz4_size );
#endif
	
	int deltasize = bitmask_lz4_size + ss.str().size();
	
	printf( "snapshot delta size: %d\n",  deltasize );
	
	if ( deltasize < 8000 ) {
		num++;
		sum += deltasize;
		if ( deltasize > maxd ) {
			maxd = deltasize;
		}
		
		printf( "avg: %d max: %d\n", sum/num, maxd );
	}
	
}
