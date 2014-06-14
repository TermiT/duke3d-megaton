// mmulti.h

#ifndef __mmulti_h__
#define __mmulti_h__

#include "csteam.h"

#define MAXMULTIPLAYERS 16

extern long myconnectindex, numplayers;
extern long connecthead, connectpoint2[MAXMULTIPLAYERS];
extern char syncstate;

long initmultiplayers_steam(steam_id_t my_steam_id, int num_steam_players, steam_id_t *player_ids, int mode);

long initmultiplayersparms(long argc, char **argv);
long initmultiplayerscycle(void);

void initmultiplayers(long argc, char **argv, char damultioption, char dacomrateoption, char dapriority);
void setpackettimeout(long datimeoutcount, long daresendagaincount);
void uninitmultiplayers(void);
void sendlogon(void);
void sendlogoff(void);
long getoutputcirclesize(void);
void setsocket(long newsocket);
void sendpacket(long other, char *bufptr, long messleng);
long getpacket(long *other, char *bufptr);
void flushpackets(void);
void genericmultifunction(long other, char *bufptr, long messleng, long command);
long isvalidipaddress(char *st);

int  sendraw(long other, char *buffer, int size, int reliable);
void oobmessage(long other, char *buffer, int size);

/* megaton-specific things */
void checkping();
void netcleanup();
void checktimeout();
void dosendpackets (long other, int timeout);

void resettimeout();

void kickplayer(long other); // arrange fake quit packet

typedef enum {
    SC_TIMELIMIT = 1,
    SC_QUIT = 2,
	SC_KICKED = 3,
} stopcode_t;

void sendstop(char code);
char *getstopmsg(int code);

#endif	// __mmulti_h__
