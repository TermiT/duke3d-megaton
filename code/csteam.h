//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef CSTEAM_H
#define CSTEAM_H

#ifdef __cplusplus
extern "C" {
#endif
    
#define MAX_CLOUD_FILES 41
#define MAX_CLOUD_FILE_LENGTH 15
#define MAX_CLOUD_FILE_SIZE 1024 * 1024  
    
int CSTEAM_Init(void);
void CSTEAM_Shutdown(void);
void CSTEAM_ShowOverlay(const char *dialog);
int CSTEAM_Online();
void CSTEAM_AchievementsInit();
const char * CSTEAM_GetUsername ();
int CSTEAM_GetStat(const char * statID);
void CSTEAM_SetStat(const char* statID, int number);
void CSTEAM_UnlockAchievement(const char * achievementID);
void CSTEAM_IndicateProgress(const char * achievementID, int currentNumber, int maxNumber);
void CSTEAM_UploadFile(const char * filename);
void CSTEAM_DownloadFile(const char * filename);
void CSTEAM_OpenCummunityHub(void);
void CSTEAM_DeleteCloudFile(const char * filename);

void CSTEAM_RunFrame(void);
    
typedef unsigned long long steam_id_t;
    
#pragma pack(push,1)
typedef struct {
    char name[64];
    steam_id_t id;
    int owner;
} player_info_t;

#define MPVERSION 90
    
typedef struct {
    int version;
    steam_id_t id;
    steam_id_t server;
    int num_players;
    player_info_t players[8];
    player_info_t *owner;
    /* game parameters */
    char name[50]; /* lobby name */
    int mode; /* 0 - dm, 1 - tdm, 2 - coop */
    int maxplayers; /* number of players */
    int usermap; /* 0 for builtin map, 1 for custom map */
    char mapname[50]; /* map to start */
    unsigned long maphash; /* crc32 for usermap */
    unsigned long grpsig; /* GRP file set signature */
    int fraglimit;
    int timelimit;
    int friendly_fire;
    int monster_skill; /* 0 - no monsters */
    int private_;
    char password[20]; /* lobby password, is unused yet */
    int markers; /* respawn markers */
    int addon; /* addon id */
    steam_id_t workshop_item_id;
	int session_token;
} lobby_info_t;
    
typedef struct {
    steam_id_t item_id;
    char title[128 + 1];		// title of item
    char description[8000];     // description of item
    char tags[1024 + 1];		// comma separated list of all tags associated with this file
    char filename[260];			// filename of the primary file (zip)
    char itemname[260];         // actual item name (map, demo)
    char path[2048];             // full path to the primary file
} workshop_item_t;


#pragma pack(pop)
    
const char* lobbyInfoToString(lobby_info_t *lobby_info);
void setLobbyInfoFromString(lobby_info_t *lobby_info, const char *value);
    
typedef struct {
    steam_id_t from;
    unsigned int pongval;
    int failed;
} pong_t;
    
typedef struct {
    steam_id_t lobby_id;
    steam_id_t from;
    char text[400];
} lobby_message_t;
    
typedef enum {
    N_NONE = 0,
    N_LOBBY_INVITE,
    N_LOBBY_ENTER,
    N_LOBBY_DATA_UPDATE,
    N_LOBBY_CHAT_UPDATE,
    N_LOBBY_CHAT_MSG,
    N_LOBBY_GAME_CREATED,
    N_LOBBY_MATCH_LIST,
    N_LOBBY_KICKED,
    N_LOBBY_CREATED,
    N_PONG,
	N_WORKSHOP_UPDATE,
	N_WORKSHOP_SUBSCRIBED,
    N_BIGPICTURE_GAMEPAD_TEXT_UPDATE,
} CSTEAM_Notification_t;

typedef void (*CSTEAM_NotificationHandler_t)(CSTEAM_Notification_t notification, int status, void *data, void *context);

void CSTEAM_SetNotificationCallback(CSTEAM_NotificationHandler_t handler, void *context);

void CSTEAM_CreateLobby(lobby_info_t *lobby_info);
void CSTEAM_UpdateLobbyList();
void CSTEAM_JoinLobby(steam_id_t lobby_id);
void CSTEAM_StartLobby(steam_id_t lobby_id);
void CSTEAM_LeaveLobby(steam_id_t lobby_id);
void CSTEAM_InviteFriends(steam_id_t lobby_id);
    
int CSTEAM_ShowGamepadTextInput (const char * defaultText, int maxChars);
int CSTEAM_GetEnteredGamepadTextLength ();
int CSTEAM_GetEnteredGamepadTextInput (char *pchText, int cchText);
    
steam_id_t CSTEAM_GetOwnLobbyId();
steam_id_t CSTEAM_LobbyOwner(steam_id_t lobby_id);
steam_id_t CSTEAM_MyID();

int  CSTEAM_NumLobbies(void);
steam_id_t CSTEAM_GetLobbyId(int index);
int CSTEAM_GetLobbyInfo(steam_id_t lobby_id, lobby_info_t *lobby_info);
    
void CSTEAM_SendLobbyMessage(steam_id_t lobby_id, const char *message);
    
void CSTEAM_UpdateWorkshopItems();
void CSTEAM_GetWorkshopItemByIndex(int index, workshop_item_t * item);
void CSTEAM_GetWorkshopItemByID(steam_id_t item_id, workshop_item_t * item);
int CSTEAM_NumWorkshopItems();
void CSTEAM_SubscribeItem(steam_id_t item_id);
void CSTEAM_UnsubscribeItem(steam_id_t item_id);
    
/* P2P networking API */
int  CSTEAM_SendPacket(steam_id_t peer_id, const void *buffer, unsigned int bufsize, int reliable, int channel);
int  CSTEAM_IsPacketAvailable(unsigned int *bufsize, int channel);
int  CSTEAM_ReadPacket(void *buffer, unsigned int bufsize, unsigned int *msgsize, steam_id_t *remote, int channel);
void CSTEAM_CloseP2P(steam_id_t remote);
void CSTEAM_DrainQueue();
    
void CSTEAM_SetPlayedWith(steam_id_t playerid);

    
const char* CSTEAM_FormatId(steam_id_t steam_id);
    
#ifdef __cplusplus
}
#endif

#endif /* CSTEAM_H */
