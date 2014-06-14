#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csteam.h"
#include "dnSnapshot.h"
#include "dnMulti.h"
#include "dnAPI.h"

#ifdef KSFORBUILD
# include "baselayer.h"
# define printf initprintf
#endif

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#define MAXPLAYERS 16
#define MAXPAKSIZ 1024*1024 //576

#define NETWORK_TIMEOUT 3000 // milliseconds

#define SEND_RELIABLE 1

#define SIMPLE_NET 0

//#define PAKRATE 40  //Packet rate/sec limit ... necessary?
#define PAKRATE 100

long myconnectindex, numplayers;
long connecthead, connectpoint2[MAXPLAYERS];

static long tims, lastsendtims[MAXPLAYERS], lastreadtims[MAXPLAYERS], fakequitflag[MAXPLAYERS], kickedflag[MAXPLAYERS];
static char pakbuf[MAXPAKSIZ];

#define FIFSIZ 512 //16384/40 = 6min:49sec
static long ipak[MAXPLAYERS][FIFSIZ], icnt0[MAXPLAYERS];
static long opak[MAXPLAYERS][FIFSIZ], ocnt0[MAXPLAYERS], ocnt1[MAXPLAYERS];
static char pakmem[4194304]; static long pakmemi = 1;

#define NETPORT 0x5bd9

static steam_id_t myid, otherid[MAXPLAYERS] = { 0 };
static steam_id_t snatchid = 0;
static long danetmode = 255, netready = 0;
static long player_ready[MAXPLAYERS] = { 0 };

static void initmultiplayers_reset(void);

void netuninit () {
    initmultiplayers_reset();
}

long netinit (long portnum) {
	return 0;
}

long netsend(long other, char *dabuf, long bufsiz) { //0:buffer full... can't send
#if 0
    steam_id_t remote = otherid[other];
        
    if (remote == 0) {
        return 0;
    }
    
    return CSTEAM_SendPacket(remote, dabuf, bufsiz, SEND_RELIABLE, 0);
#else
	return dnSendPacket( other, dabuf, bufsiz );
#endif
}

void kickplayer(long other) {
	char buf[3];
	if ( numplayers > 1 && other >= 0 && other < numplayers && myconnectindex == connecthead && other != myconnectindex && kickedflag[other] == 0) {
		Sys_DPrintf( "[DUKEMP] actually kicking player #%d\n", other );
		fakequitflag[other] = 1;
		kickedflag[other] = 1;
//		buf[0] = 254;
//		buf[1] = SC_KICKED;
//		sendpacket( other, buf, 2 );
	}
}

void resettimeout(void) {
	memset( lastreadtims, 0, sizeof( lastreadtims ) );
}

void checktimeout(void) {
	int i;
	long now;
	long timeout;
	
	if ( !dnIsInMultiMode() ) {
		return;
	}
	
	timeout = NETWORK_TIMEOUT;
	
	if ( !dnIsHost() ) {
		timeout *= 2; // clients wait for server twice longer
	}
	
	now = getticks();
	
	if ( connecthead == myconnectindex ) {
		lastreadtims[connecthead] = now;
	}
	
	for ( i = 0; i < numplayers; i++ ) {
		if ( lastreadtims[i] != 0 && ( lastreadtims[i] + timeout < now ) && kickedflag[i] == 0 ) {
			
			Sys_DPrintf( "[DUKEMP] Player#%d is timed out\n", i );
			
			if ( i == connecthead ) {
				gameexit( "Server timed out" );
			} else {
				if ( myconnectindex != i ) {
					kickplayer( i );
				}
			}
		}
	}
}

long netread (long *other, char *dabuf, long bufsiz) { //0:no packets in buffer
#if 0
	long i, from;
    unsigned int bufsize, msgsize;

    if ( !CSTEAM_IsPacketAvailable(&bufsize, CHAN_LEGACY ) ) {
        return 0;
    }
    
    if ( !CSTEAM_ReadPacket(pakbuf, bufsize, &msgsize, &snatchid, 0) ) {
        return 0;
    }
	
	if ( msgsize > bufsiz ) {
		return 0;
	}
	
	memcpy( dabuf, pakbuf, msgsize );
    
#if (SIMMIS > 0)
	if ((rand()&255) < SIMMIS) {
        return 0;
    }
#endif
        
	*other = myconnectindex;
	for (i = 0; i < MAXPLAYERS; i++) {
		if (otherid[i] == snatchid) {
            *other = i;
            break;
        }
    }

#if (SIMLAG > 1)
	i = simlagcnt[*other]%(SIMLAG+1);
	*(short *)&simlagfif[*other][i][0] = bufsiz; memcpy(&simlagfif[*other][i][2],dabuf,bufsiz);
	simlagcnt[*other]++; if (simlagcnt[*other] < SIMLAG+1) return(0);
	i = simlagcnt[*other]%(SIMLAG+1);
	bufsiz = *(short *)&simlagfif[*other][i][0]; memcpy(dabuf,&simlagfif[*other][i][2],bufsiz);
#endif

	if ( *other >= 0 && *other < MAXPLAYERS ) {
		lastreadtims[*other] = getticks();
	}

	return msgsize;
#else
	int playerIndex, retval;
	retval = dnGetPacket( &playerIndex, dabuf, bufsiz );
	snatchid = *other = playerIndex;
	if ( dnIsPlayerIndexValid( playerIndex ) ) {
		lastreadtims[playerIndex] = getticks();
	}
	return retval;
#endif
}

long isvalidipaddress (char *st) {
    // TODO invent some kind of new Steam addressing scheme
    return 1;
}

//---------------------------------- Obsolete variables&functions ----------------------------------
char syncstate = 0;
void setpackettimeout (long datimeoutcount, long daresendagaincount) {}
void genericmultifunction (long other, char *bufptr, long messleng, long command) {}
long getoutputcirclesize () { return(0); }
void setsocket (long newsocket) { }
void flushpackets () {}
void sendlogon () {}
void sendlogoff () {}
//--------------------------------------------------------------------------------------------------

static long crctab16[256];
static void initcrc16() {
	long i, j, k, a;
	for (j = 0; j < 256; j++) {
		for (i = 7, k = (j<<8), a = 0; i >= 0; i--, k = ((k<<1)&65535)) {
			if ((k^a)&0x8000) {
                a = ((a<<1)&65535)^0x1021;
            } else {
                a = ((a<<1)&65535);
            }
		}
		crctab16[j] = (a&65535);
	}
}

#define updatecrc16(crc,dat) crc = (((crc<<8)&65535)^crctab16[((((unsigned short)crc)>>8)&65535)^dat])
static unsigned short getcrc16(char *buffer, long bufleng) {
	long i, j;

	j = 0;
	for (i = bufleng-1; i >= 0; i--) {
        updatecrc16(j,buffer[i]);
    }
	return (unsigned short)(j&65535);
}

void uninitmultiplayers() {
    netuninit();
}

long getpacket(long *, char *);

static void initmultiplayers_reset(void) {
	long i;
    
	initcrc16();
	memset(icnt0, 0, sizeof(icnt0));
	memset(ocnt0, 0, sizeof(ocnt0));
	memset(ocnt1, 0, sizeof(ocnt1));
	memset(ipak, 0, sizeof(ipak));
	memset(opak, 0, sizeof(opak));
	memset(pakmem, 0, sizeof(pakmem));
    
#if (SIMLAG > 1)
	memset(simlagcnt,0,sizeof(simlagcnt));
#endif
    
	lastsendtims[0] = getticks();
	for(i = 1; i < MAXPLAYERS; i++) {
        lastsendtims[i] = lastsendtims[0];
		lastreadtims[i] = 0;
    }
	numplayers = 1;
    myconnectindex = 0;
    
	memset(otherid, 0, sizeof(otherid));
    memset(player_ready, 0, sizeof(player_ready));
	memset(fakequitflag, 0, sizeof(fakequitflag));
	memset(kickedflag, 0, sizeof(kickedflag));
}

void fakequit(long other) {
	fakequitflag[other] = 1;
}

long initmultiplayers_steam(steam_id_t my_steam_id, int num_steam_players, steam_id_t *player_ids, int mode) {
    int i;
    
    initmultiplayers_reset();
    
    if (num_steam_players > MAXPLAYERS) {
        num_steam_players = MAXPLAYERS;
    }
    
    myid = my_steam_id;
    numplayers = num_steam_players;
    memcpy(otherid, player_ids, num_steam_players*sizeof(steam_id_t));
    
    danetmode = mode; /* 0 = M/S mode, 1 = P2P mode */
    connecthead = 0;
	for (i = 0; i < numplayers - 1; i++) {
        connectpoint2[i] = i + 1;
    }
	connectpoint2[numplayers-1] = -1;
    
    for (i = 0; i < numplayers; i++) {
        if (otherid[i] == myid) {
            myconnectindex = i;
        }
    }
	
	clearfifo();

    return 0;
}

long initmultiplayersparms(long argc, char **argv) {
    return 0;
}

long initmultiplayerscycle(void) {
	long i, k;

	getpacket(&i, 0);

	tims = (long)Sys_GetTicks();
	if (myconnectindex == connecthead) {
		for (i = numplayers - 1; i > 0; i--) {
			if (!player_ready[i]) {
                break;
            }
        }
		if (i == 0) {
			netready = 1;
			return 0;
		}
	}
	else {
		if (netready) {
            return 0;
        }
		if (tims < lastsendtims[connecthead]) {
            lastsendtims[connecthead] = tims;
        }
		if (tims >= lastsendtims[connecthead]+250) { //1000/PAKRATE)
			lastsendtims[connecthead] = tims;
				//   short crc16ofs;       //offset of crc16
				//   long icnt0;           //-1 (special packet for MMULTI.C's player collection)
				//   ...
				//   unsigned short crc16; //CRC16 of everything except crc16
			k = 2;
			*(long *)&pakbuf[k] = -1; k += 4;
			pakbuf[k++] = 0xaa;
			*(unsigned short *)&pakbuf[0] = (unsigned short)k;
			*(unsigned short *)&pakbuf[k] = getcrc16(pakbuf,k); k += 2;
			netsend(connecthead,pakbuf,k);
		}
	}

	return 1;
}

void initmultiplayers (long argc, char **argv, char damultioption, char dacomrateoption, char dapriority) {
    /* obsolette */
}

void dosendpackets (long other, int timeout) {
	long i, j, k;

	if (otherid[other] == 0) {
        return;
    }

		//Packet format:
		//   short crc16ofs;       //offset of crc16
		//   long icnt0;           //earliest unacked packet
		//   char ibits[32];       //ack status of packets icnt0<=i<icnt0+256
		//   while (short leng)    //leng: !=0 for packet, 0 for no more packets
		//   {
		//      long ocnt;         //index of following packet data
		//      char pak[leng];    //actual packet data :)
		//   }
		//   unsigned short crc16; //CRC16 of everything except crc16


	tims = (long)Sys_GetTicks();
	if (tims < lastsendtims[other]) {
        lastsendtims[other] = tims;
    }
	if (tims < lastsendtims[other]+timeout) {
        return;
    }
	lastsendtims[other] = tims;

	k = 2;
	*(long *)&pakbuf[k] = icnt0[other];
    k += 4;
	memset(&pakbuf[k],0,32);
	for (i = icnt0[other]; i < icnt0[other]+256; i++) {
		if (ipak[other][i&(FIFSIZ-1)]) {
			pakbuf[((i-icnt0[other])>>3)+k] |= (1<<((i-icnt0[other])&7));
        }
    }
	k += 32;

	while ((ocnt0[other] < ocnt1[other]) && (!opak[other][ocnt0[other]&(FIFSIZ-1)])) {
        ocnt0[other]++;
    }
	for (i = ocnt0[other]; i < ocnt1[other]; i++) {
		j = *(short *)&pakmem[opak[other][i&(FIFSIZ-1)]];
        if (!j) {
            continue; //packet already acked
        }
		if (k+6+j+4 > (long)sizeof(pakbuf)) {
            break;
        }
		*(unsigned short *)&pakbuf[k] = (unsigned short)j;
        k += 2;
		*(long *)&pakbuf[k] = i;
        k += 4;
		memcpy(&pakbuf[k],&pakmem[opak[other][i&(FIFSIZ-1)]+2],j);
        k += j;
	}
	*(unsigned short *)&pakbuf[k] = 0;
    k += 2;
	*(unsigned short *)&pakbuf[0] = (unsigned short)k;
	*(unsigned short *)&pakbuf[k] = getcrc16(pakbuf,k);
    k += 2;

	//printf("Send: "); for(i=0;i<k;i++) printf("%02x ",pakbuf[i]); printf("\n");
	netsend(other, pakbuf, k);
}

void sendpacket (long other, char *bufptr, long messleng) {
	
#if SIMPLE_NET
	return dnSendPacket( other, bufptr, messleng );
#endif
	
	if (numplayers < 2) {
        return;
    }
	
	if (pakmemi+messleng+2 > (long)sizeof(pakmem)) {
        pakmemi = 1;
    }
	opak[other][ocnt1[other]&(FIFSIZ-1)] = pakmemi;
	*(short *)&pakmem[pakmemi] = messleng;
	memcpy(&pakmem[pakmemi+2],bufptr,messleng);
    pakmemi += messleng+2;
	ocnt1[other]++;

	//printf("Send: "); for(i=0;i<messleng;i++) printf("%02x ",bufptr[i]); printf("\n");

	dosendpackets(other, 1000/PAKRATE);
}

void netcleanup() {
    int i;
    
    CSTEAM_DrainQueue();
    
#if 1
    for ( i = 0; i < MAXPLAYERS; i++ ) {
        if (otherid[i] != 0) {
            CSTEAM_CloseP2P(otherid[i]);
            otherid[i] = 0;
        }
    }
#endif
}

char *getstopmsg(int code) {
    switch (code) {
//        case 1: return "Timelimit hit";
        case 2: return "Server quit";
		case 3: return "Kicked";
    };
    return " ";
}

void sendstop(char code) {
    char packbuf[2];
    int i;
    
    Sys_DPrintf("[MP] Sending termination packet\n");	
    
    packbuf[0] = 254;
    packbuf[1] = code;
    for (i = connectpoint2[connecthead]; i >= 0; i = connectpoint2[i]) {
        sendpacket(i, packbuf, 2);
    }
}

	//passing bufptr == 0 enables receive&sending raw packets but does not return any received packets
	//(used as hack for player collection)
long getpacket (long *retother, char *bufptr) {
	long i, j, k, ic0, crc16ofs, messleng, other;

#if SIMPLE_NET
	int playerIndex, retval;
	
	retval = dnGetPacket( &playerIndex, bufptr, MAXPAKSIZ );
	*retother = playerIndex;
	return retval;
#endif
	
	if ( !dnIsInMultiMode() ) {
		return 0;
	}
	
	*retother = -1;

	/* check for arranged fake quit packets */
	
	for ( i = connecthead; i >= 0; i = connectpoint2[i] ) {
		if ( fakequitflag[i] ) {
			
			Sys_DPrintf( "[DUKEMP] fake quit packet for %d\n", i );
			
			fakequitflag[i] = 0;
			*retother = i;
			bufptr[0] = 1;
			bufptr[1] = 64;
			bufptr[2] = 4;
			return 3;
		}
	}

#if 0
	if (netready)
#else 
	if (1)
#endif
	{
		for (i = connecthead; i >= 0; i = connectpoint2[i]) {
			if (i != myconnectindex) {
                dosendpackets(i, 1000/PAKRATE);
            }
			if ((!danetmode) && (myconnectindex != connecthead)) {
                break; //slaves in M/S mode only send to master
            }
		}
	}

	while ( (messleng = netread(&other, pakbuf, sizeof(pakbuf))) ) {
//		if ( messleng > 1500 ) {
//			return messleng;
//		}
		
		if ( other == -1 ) {
			continue;
		}
			//Packet format:
			//   short crc16ofs;       //offset of crc16
			//   long icnt0;           //earliest unacked packet
			//   char ibits[32];       //ack status of packets icnt0<=i<icnt0+256
			//   while (short leng)    //leng: !=0 for packet, 0 for no more packets
			//   {
			//      long ocnt;         //index of following packet data
			//      char pak[leng];    //actual packet data :)
			//   }
			//   unsigned short crc16; //CRC16 of everything except crc16
		k = 0;
		crc16ofs = (long)(*(unsigned short *)&pakbuf[k]);
        k += 2;

		if ((crc16ofs+2 <= (long)sizeof(pakbuf)) && (getcrc16(pakbuf,crc16ofs) == (*(unsigned short *)&pakbuf[crc16ofs]))) {
			ic0 = *(long *)&pakbuf[k];
            k += 4;
			if (ic0 == -1) {
					 //Slave sends 0xaa to Master at initmultiplayers() and waits for 0xab response
					 //Master responds to slave with 0xab whenever it receives a 0xaa - even if during game!
				if ((pakbuf[k] == 0xaa) && (myconnectindex == connecthead)) {
					for(other = 1; other < numplayers; other++) {
							//Only send to others asking for a response
						if ((otherid[other]) && ((otherid[other] != snatchid))) {
                            continue;
                        }
						otherid[other] = snatchid;
                        player_ready[other] = 1;

							//   short crc16ofs;       //offset of crc16
							//   long icnt0;           //-1 (special packet for MMULTI.C's player collection)
							//   ...
							//   unsigned short crc16; //CRC16 of everything except crc16
						k = 2;
						*(long *)&pakbuf[k] = -1; k += 4;
						pakbuf[k++] = 0xab;
						pakbuf[k++] = (char)other;
						pakbuf[k++] = (char)numplayers;
						*(unsigned short *)&pakbuf[0] = (unsigned short)k;
						*(unsigned short *)&pakbuf[k] = getcrc16(pakbuf,k); k += 2;
						netsend(other,pakbuf,k);
						break;
					}
				} else if ((pakbuf[k] == 0xab) && (myconnectindex != connecthead)) {
					if (((unsigned long)pakbuf[k+1] < (unsigned long)pakbuf[k+2]) &&
						 ((unsigned long)pakbuf[k+2] < (unsigned long)MAXPLAYERS)) {
						myconnectindex = (long)pakbuf[k+1];
						numplayers = (long)pakbuf[k+2];

						connecthead = 0;
						for (i = 0; i < numplayers-1; i++) {
                            connectpoint2[i] = i+1;
                        }
						connectpoint2[numplayers-1] = -1;

						otherid[connecthead] = snatchid;
						netready = 1;
					}
				}
			}
			else {
				if (ocnt0[other] < ic0) {
                    ocnt0[other] = ic0;
                }
				for (i = ic0; i < min(ic0+256, ocnt1[other]); i++) {
					if (pakbuf[((i-ic0)>>3)+k]&(1<<((i-ic0)&7))) {
						opak[other][i&(FIFSIZ-1)] = 0;
                    }
                }
				k += 32;

				messleng = (long)(*(unsigned short *)&pakbuf[k]); k += 2;
				while (messleng) {
					j = *(long *)&pakbuf[k]; k += 4;
					if ((j >= icnt0[other]) && (!ipak[other][j&(FIFSIZ-1)])) {
						if (pakmemi+messleng+2 > (long)sizeof(pakmem)) {
                            pakmemi = 1;
                        }
						ipak[other][j&(FIFSIZ-1)] = pakmemi;
						*(short *)&pakmem[pakmemi] = messleng;
						memcpy(&pakmem[pakmemi+2], &pakbuf[k], messleng);
                        pakmemi += messleng+2;
					}
					k += messleng;
					messleng = (long)(*(unsigned short *)&pakbuf[k]);
                    k += 2;
				}
			}
		}
	}

		//Return next valid packet from any player
	if (!bufptr)  {
        return 0;
    }
	for (i = connecthead; i >= 0; i = connectpoint2[i]) {
		if (i != myconnectindex) {
			j = ipak[i][icnt0[i]&(FIFSIZ-1)];
			if (j) {
				messleng = *(short*)&pakmem[j];
                memcpy(bufptr, &pakmem[j+2], messleng);
				*retother = i;
                ipak[i][icnt0[i]&(FIFSIZ-1)] = 0;
                icnt0[i]++;
				return messleng;
			}
		}
		if ((!danetmode) && (myconnectindex != connecthead)) {
            break; //slaves in M/S mode only send to master
        }
	}
	return 0;
}
