//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "csteam.h"
#include <steam/steam_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "steam/steamclientpublic.h"
#include "steam/isteamfriends.h"
#include "steam/isteammatchmaking.h"
#include "steam/isteamutils.h"
#include "steam/isteamugc.h"
#include "steam/steam_gameserver.h"
#include "base64.h"
#include "helpers.h"
#include <sys/stat.h>
#include <stdio.h>


#ifndef _WIN32
#include <unistd.h>
#define Dmkdir(x) mkdir(x, 0777)
#define Dgetcwd(x, y) getcwd(x, y)
#else
#include <direct.h>
#define Dmkdir(x) _mkdir(x)
#define Dgetcwd(x, y) _getcwd(x, y)
#endif


#include <vector>
#include <stdlib.h>

#include "compat.h"
#include "miniz.h"

extern "C" {
void Sys_DPrintf(const char *format, ...);
const char* Sys_GetResourceDir(void);
}


void setLobbyInfoFromString(lobby_info_t *lobby_info, const char *value);

const char* lobbyInfoToString(lobby_info_t *lobby_info) {
    static char buffer[sizeof(lobby_info_t)*2];
    char *encoded;
    int len;
    
    encoded = base64_encode(lobby_info, sizeof(*lobby_info), &len);
    memcpy(buffer, encoded, len);
    free(encoded);
    buffer[len] = 0;
    
    Sys_DPrintf("lobby_info_t::toString(): sizeof(lobby_info_t)=%d\n", sizeof(lobby_info_t));
    Sys_DPrintf("lobby_info_t::toString(): length of encoded data: %d\n", len);
    
    return buffer;
}


void setLobbyInfoFromString(lobby_info_t *lobby_info, const char *value) {
    int len;
    unsigned char *decoded_data = base64_decode(value, strlen(value), &len);
    if (len != sizeof(*lobby_info)) {
        Sys_DPrintf("Lobby data size mismatch during unserializing, not set\n");
        Sys_DPrintf("lobby_info_t::setFromString(): sizeof(lobby_info_t)=%d\n", sizeof(lobby_info_t));
        Sys_DPrintf("lobby_info_t::setFromString(): length of input data: %d\n", strlen(value));
        memset((void*)lobby_info, 0, sizeof(*lobby_info));
    } else {
        memcpy((void*)lobby_info, (void*)decoded_data, len);
    }
    free(decoded_data);
}


static int
lobby_cmp(const void *p1, const void *p2) {
    player_info_t *pl1 = (player_info_t*)p1;
    player_info_t *pl2 = (player_info_t*)p2;
    if (pl1->id == pl2->id) {
        return 0;
    }
    if (pl1->id < pl2->id) {
        return -1;
    }
    return 1;
}

static void
printLobbyData(CSteamID lobbyId) {
#if 0
    int data_count = SteamMatchmaking()->GetLobbyDataCount(lobbyId);
    static char key[100];
    static char value[1024];
    for (int i = 0; i < data_count; i++) {
        SteamMatchmaking()->GetLobbyDataByIndex(lobbyId, i, key, sizeof(key), value, sizeof(value));
        Sys_DPrintf("data[%d]: '%s' = '%s'\n", i, key, value);
    }
#endif
}

bool operator == (const workshop_item_t& a, const workshop_item_t& b) {
    return a.item_id == b.item_id &&
        !strcmp(a.description, b.description) &&    
        !strcmp(a.filename, b.filename) &&
        !strcmp(a.itemname, b.itemname) &&
        !strcmp(a.tags, b.tags) &&
        !strcmp(a.title, b.title);
}

bool operator != (const workshop_item_t& a, const workshop_item_t& b) {
    return !(a == b);
}

const AppId_t app_id = 225140;

/* OverlayManager */

extern "C" {
    void getgamepad(void);
    void closegamepad(void);
}

struct OverlayManager {
    CSTEAM_NotificationHandler_t handler;
	void *context;

    void setHandler(CSTEAM_NotificationHandler_t handler, void *context) {
        this->handler = handler;
        this->context = context;
    }
    
    OverlayManager():overlay_update_callback(this, &OverlayManager::OnOverlayStateChanged),
    bigpicture_text_update(this, &OverlayManager::OnBigPictureTextSubmited), handler(NULL), context(NULL){}
    STEAM_CALLBACK(OverlayManager, OnOverlayStateChanged, GameOverlayActivated_t, overlay_update_callback) {
        if (pParam->m_bActive) {
            closegamepad();
        } else {
            getgamepad();
        }
    }
    
    STEAM_CALLBACK(OverlayManager, OnBigPictureTextSubmited, GamepadTextInputDismissed_t, bigpicture_text_update) {
        char *text = NULL;
        int textLength = 0;
        if (handler) {
            textLength = CSTEAM_GetEnteredGamepadTextLength();
            if (pParam->m_bSubmitted && (textLength) > 0) {
                text = (char *)malloc(sizeof(char)*textLength);
                CSTEAM_GetEnteredGamepadTextInput(text, textLength+1);
                text[textLength+1]='\0';
                handler(N_BIGPICTURE_GAMEPAD_TEXT_UPDATE, 1, text, context);
//                free(text);
            }
        }
    }
};

static OverlayManager *overlayManager = 0;

/* Workshop manager */
struct WorkshopManager {
	CSTEAM_NotificationHandler_t handler;
	void *context;

    bool refreshing;
    std::vector<workshop_item_t> display_items, items;
    
	void setHandler(CSTEAM_NotificationHandler_t handler, void *context) {
        this->handler = handler;
        this->context = context;
    }
    

    WorkshopManager():refreshing(false),handler(NULL) {
    }
    
    CCallResult<WorkshopManager, SteamUGCQueryCompleted_t> ugc_query_completed_callback;
    CCallResult<WorkshopManager, SteamUGCRequestUGCDetailsResult_t> ugc_request_result;
    CCallResult<WorkshopManager, RemoteStorageDownloadUGCResult_t> ugc_download_result;
    CCallResult<WorkshopManager, RemoteStorageSubscribePublishedFileResult_t>ugc_subscribe_result;
    CCallResult<WorkshopManager, RemoteStorageUnsubscribePublishedFileResult_t>ugc_unsubscribe_result;
        
    void onSubscribe(RemoteStorageSubscribePublishedFileResult_t *pCallback, bool failure) {
		if (handler) {
			workshop_item_t item = { 0 };
			item.item_id = pCallback->m_nPublishedFileId;
			handler(N_WORKSHOP_SUBSCRIBED, failure ? 1 : 0, (void*)&item, context);
		}
		
        if (failure) {
            Sys_DPrintf( "onSubscribe failed\n" );
            return;
        }
        this->RefreshSubscribedItems();
    }
    
    void onUnsubscribe(RemoteStorageUnsubscribePublishedFileResult_t *pCallback, bool failure) {
        if (failure) {
            Sys_DPrintf( "onUnsubscribe failed\n" );
            return;
        }
        this->RefreshSubscribedItems();
    }
    
    void onFileDownloadCompleted(RemoteStorageDownloadUGCResult_t *pCallback, bool failure) {
        if (failure) {
            printf("onFileDownloadCompleted failed\n");
            return;
        }
        workshop_item_t item = GetItemByFilename(pCallback->m_pchFileName);
        unpack_music(item.item_id);
    }
    
    void onQueryCompleted (SteamUGCQueryCompleted_t *pCallback, bool failure) {
        char item_folder[2048];
        char zipfile[2048];
        if (failure) {
			if (handler) {
				handler(N_WORKSHOP_UPDATE, 1, NULL, context);
			}
            Sys_DPrintf("onQueryCompleted failed\n");
            return;
        }
        SteamUGCDetails_t details;
        for (int i = 0; i < pCallback->m_unNumResultsReturned; i++) {
            SteamUGC()->GetQueryUGCResult(pCallback->m_handle, i, &details);
            if (details.m_bBanned || strstr(details.m_rgchTags, "Map") == NULL)
                continue;
            
            workshop_item_t item = { 0 };
            Dgetcwd(item_folder, sizeof(item_folder));
            strcat(item_folder, va("/workshop/maps/%llu",details.m_nPublishedFileId));
            bool need_download = false;
            sprintf(zipfile, "%s/%s", item_folder, details.m_pchFileName);
            
            if (Dmkdir(item_folder) == 0) { //folder created
                need_download = true;
            } else {
                if (!fopen(zipfile, "r")) {
                    need_download = true;
                } else if (get_modified_time(zipfile) < details.m_rtimeUpdated) {
                        remove(zipfile);
                        need_download = true;
                }
            }
            if (need_download) {
                SteamAPICall_t callback = SteamRemoteStorage()->UGCDownloadToLocation(details.m_hFile, zipfile, i);
                ugc_download_result.Set(callback, this, &WorkshopManager::onFileDownloadCompleted);
            }
            strcpy(item.title, details.m_rgchTitle);
            strcpy(item.description, details.m_rgchDescription);
            strcpy(item.tags, details.m_rgchTags);
            strcpy(item.filename, details.m_pchFileName);
            strcpy(item.itemname, str_replace(details.m_pchFileName, ".zip", ".map"));
            strcpy(item.path, item_folder);
            item.item_id = details.m_nPublishedFileId;
            items.push_back(item);
        }
        SteamUGC()->ReleaseQueryUGCRequest(pCallback->m_handle);
        if (pCallback->m_unNumResultsReturned == kNumUGCResultsPerPage) {
            RequestItems(items.size()/kNumUGCResultsPerPage + 1);
        } else {
            if (display_items.size() != items.size() || display_items != items) {
                display_items = items;
                if (handler) {
					handler(N_WORKSHOP_UPDATE, 0, NULL, context);
                }
            }
            refreshing = false;
        }
        //unpack_music(198137351);
    }
    
    void RefreshSubscribedItems() {
        if (!CSTEAM_Online() || refreshing) return;
        refreshing = true;
        items.clear();
        RequestItems(1);
    }
    
    void RequestItems(int num) {
        CSteamID steam_id = SteamUser()->GetSteamID();
        UGCQueryHandle_t query_handle = SteamUGC()->CreateQueryUserUGCRequest(steam_id.GetAccountID(), k_EUserUGCList_Subscribed, k_EUGCMatchingUGCType_Items, k_EUserUGCListSortOrder_TitleAsc, app_id, app_id, num);
        SteamAPICall_t callback = SteamUGC()->SendQueryUGCRequest(query_handle);
        ugc_query_completed_callback.Set(callback, this, &WorkshopManager::onQueryCompleted);

    }
    
    void Subscribe(steam_id_t item_id) {
        if (!CSTEAM_Online()/* || refreshing*/) return;
        SteamAPICall_t callback = SteamRemoteStorage()->SubscribePublishedFile(item_id);
        ugc_subscribe_result.Set(callback, this, &WorkshopManager::onSubscribe);
    }

    void Unsubscribe(steam_id_t item_id) {
        if (!CSTEAM_Online()/* || refreshing*/) return;
        SteamAPICall_t callback = SteamRemoteStorage()->UnsubscribePublishedFile(item_id);
        ugc_unsubscribe_result.Set(callback, this, &WorkshopManager::onUnsubscribe);
    }    
    
    
    int32 GetItemsNumber() {
        return display_items.size();
    }
    
    workshop_item_t GetItemByIndex(int index) {
        return display_items[index];
    }
    
    workshop_item_t GetItemByID(steam_id_t item_id) {
        workshop_item_t item = {0};
        for (int i=0; i < display_items.size(); i++) {
            if (display_items[i].item_id == item_id) {
                memcpy(&item, &display_items[i], sizeof(workshop_item_t));
                break;
            }
        }
        return item;
    }
    
    workshop_item_t GetItemByFilename(const char * filename) {
        workshop_item_t item = {0};
        for (int i=0; i < display_items.size(); i++) {
            if (strcmp(items[i].filename, filename) == 0) {
                memcpy(&item, &display_items[i], sizeof(workshop_item_t));
                break;
            }
        }
        return item;
    }

    void unpack_music(steam_id_t item_id) {
        char zipfile[2048];
        char ogg[2048];
        char gamefolder[2048];
        mz_zip_archive pZip = {0};
        workshop_item_t item = GetItemByID(item_id);
        Dgetcwd(gamefolder, sizeof(gamefolder));
        sprintf(zipfile, "%s/%s", item.path, item.filename);
        strcpy(ogg, str_replace(item.filename, ".zip", ".ogg"));
        mz_zip_reader_init_file(&pZip, zipfile, 0);
        mz_zip_reader_extract_file_to_file(&pZip, ogg, va("%s/music/usermaps/%s", gamefolder, ogg), 0);
        mz_zip_reader_end(&pZip);
    }
};


static WorkshopManager *workshopManager = 0;


/* Lobby manager */

struct LobbyManager {
    CSTEAM_NotificationHandler_t handler;
    void *context;
    
    std::vector<CSteamID> lobby_list;
    
    lobby_info_t own_lobby_info;
    CSteamID own_lobby_id;
    CSteamID game_server;
    
    CCallResult<LobbyManager, LobbyCreated_t> lobby_created_callback;
    CCallResult<LobbyManager, LobbyMatchList_t> lobby_list_updated_callback;
    CCallResult<LobbyManager, LobbyEnter_t> lobby_join_callback;
    
    STEAM_CALLBACK(LobbyManager, onLobbyChatUpdate, LobbyChatUpdate_t, lobby_chat_update_callback) {
        if (handler) {
            lobby_info_t lobby_info;
            getLobbyInfo(pParam->m_ulSteamIDLobby, &lobby_info);
            handler(N_LOBBY_CHAT_UPDATE, 0, &lobby_info, context);
        }
        SteamMatchmaking()->RequestLobbyData(pParam->m_ulSteamIDLobby);
    }
    
    STEAM_CALLBACK(LobbyManager, onLobbyChatMessage, LobbyChatMsg_t, lobby_chat_message_callback) {
        if (handler) {
            lobby_message_t message = { 0 };
            message.lobby_id = pParam->m_ulSteamIDLobby;
            EChatEntryType entry_type;
            CSteamID from;
            SteamMatchmaking()->GetLobbyChatEntry(pParam->m_ulSteamIDLobby, pParam->m_iChatID, &from, &message.text[0], sizeof(message.text)-1, &entry_type);
            message.from = from.ConvertToUint64();
            if (entry_type == k_EChatEntryTypeChatMsg) {
                handler(N_LOBBY_CHAT_MSG, 0, &message, context);
            }
        }
    }
    
    STEAM_CALLBACK(LobbyManager, onLobbyDataUpdate, LobbyDataUpdate_t, lobby_data_update_callback) {
		Sys_DPrintf( "[DUKEMP] onLobbyDataUpdate: %s\n", CSTEAM_FormatId( pParam->m_ulSteamIDLobby ) );
        if (handler) {
            lobby_info_t lobby_info;
            getLobbyInfo(pParam->m_ulSteamIDLobby, &lobby_info);
            handler(N_LOBBY_DATA_UPDATE, 0, &lobby_info, context);
        }
#if 1
        printLobbyData(pParam->m_ulSteamIDLobby);
#endif
    }
    
    STEAM_CALLBACK(LobbyManager, onLobbyGameCreated, LobbyGameCreated_t, lobby_game_created_callback) {
        Sys_DPrintf("[DUKEMP] lobby game created\n");
        if (handler) {
            game_server = pParam->m_ulSteamIDGameServer;
            lobby_info_t lobby_info;
            getLobbyInfo(pParam->m_ulSteamIDLobby, &lobby_info);
            lobby_info.server = game_server.ConvertToUint64();
            handler(N_LOBBY_GAME_CREATED, 0, &lobby_info, context);
        }
    }
    
//    STEAM_GAMESERVER_CALLBACK(LobbyManager, OnP2PSessionRequest, P2PSessionRequest_t, server_session_request) {
//        /* allow to anyone */
//        printf(">>> accepted p2p connection\n");
//        SteamGameServerNetworking()->AcceptP2PSessionWithUser(pParam->m_steamIDRemote);
//    }
    STEAM_CALLBACK(LobbyManager, OnP2PSessionRequest, P2PSessionRequest_t, session_request) {
        steam_id_t steam_id = pParam->m_steamIDRemote.ConvertToUint64();
        SteamNetworking()->AcceptP2PSessionWithUser(pParam->m_steamIDRemote);
    }
    
    STEAM_CALLBACK(LobbyManager, OnLobbyJoinRequested, GameLobbyJoinRequested_t, join_request) {
        steam_id_t lobby_id = pParam->m_steamIDLobby.ConvertToUint64();
        lobby_info_t lobby_info = { 0 };
        if (handler) {
            getLobbyInfo(lobby_id, &lobby_info);
            handler(N_LOBBY_INVITE, 0, &lobby_info, context);
        }
    }
    
    void onLobbyCreated(LobbyCreated_t *pCallback, bool failure) {
        char buffer[20];
        if (failure) {
            Sys_DPrintf(">>> lobby failed\n");
            if (handler) {
                handler(N_LOBBY_CREATED, -1, NULL, context);
            }
        } else {
            own_lobby_id = pCallback->m_ulSteamIDLobby;
            Sys_DPrintf(">>> lobby created successfully\n");
            
            own_lobby_info.version = MPVERSION;
            own_lobby_info.server = CSTEAM_MyID();
            const char *info_str = lobbyInfoToString(&own_lobby_info);
            SteamMatchmaking()->SetLobbyData(own_lobby_id, "info", info_str);
            Sys_DPrintf(">>> Lobby data successfuly set\n");
            Sys_DPrintf("info = %s\n", info_str);
            
            lobby_info_t lobby_info;
            getLobbyInfo(pCallback->m_ulSteamIDLobby, &lobby_info);

            if (handler) {
                handler(N_LOBBY_CREATED, 0, &lobby_info, context);
            }
        }
    }
    
    void onLobbyListUpdated(LobbyMatchList_t *pCallback, bool failure) {
        if (failure) {
            Sys_DPrintf(">>> lobby list update failed\n");
            if (handler) {
                handler(N_LOBBY_MATCH_LIST, -1, NULL, context);
            }
        } else {
            lobby_list.resize(pCallback->m_nLobbiesMatching);
			Sys_DPrintf( "[DUKEMP] Lobby list has just updated\n" );
            for (int i = 0; i < pCallback->m_nLobbiesMatching; i++) {
                lobby_list[i] = SteamMatchmaking()->GetLobbyByIndex(i);
				Sys_DPrintf( "[DUKEMP] %s\n", CSTEAM_FormatId( lobby_list[i].ConvertToUint64() ) );
            }
            
            Sys_DPrintf(">>> lobby list update succeeded\n");
            if (handler) {
                handler(N_LOBBY_MATCH_LIST, 0, pCallback, context);
            }
        }
    }
    
    void onLobbyEnter(LobbyEnter_t *pCallback, bool failure) {
        if (failure) {
            Sys_DPrintf("[DUKEMP] lobby enter fail\n");
            if (handler) {
                handler(N_LOBBY_ENTER, -1, NULL, context);
            }
        } else {
            Sys_DPrintf("[DUKEMP] lobby entered successfully\n");
            lobby_info_t lobby_info;
            getLobbyInfo(pCallback->m_ulSteamIDLobby, &lobby_info);
            if (handler) {
                handler(N_LOBBY_ENTER, 0, &lobby_info, context);
            }
        }
    }
    
    void createLobby(lobby_info_t *lobby_info) {
        Sys_DPrintf("[DUKEMP] creating lobby...\n");
        
        memcpy(&own_lobby_info, lobby_info, sizeof(lobby_info_t));
        own_lobby_id.SetFromUint64(0);
        SteamAPICall_t call = SteamMatchmaking()->CreateLobby(lobby_info->private_ ? k_ELobbyTypePrivate : k_ELobbyTypePublic, lobby_info->maxplayers);
        lobby_created_callback.Set(call, this, &LobbyManager::onLobbyCreated);
    }
    
    void updateLobbyList() {
        Sys_DPrintf("[DUKEMP] updating lobby list\n");

        lobby_list.clear();
        SteamAPICall_t call = SteamMatchmaking()->RequestLobbyList();
        lobby_list_updated_callback.Set(call, this, &LobbyManager::onLobbyListUpdated);
    }
    
    int  numLobbies() {
        return lobby_list.size();
    }
    
    CSteamID getLobbyByIndex(int index) {
        if (index >= 0 && index < lobby_list.size()) {
            return lobby_list[index];
        }
        return CSteamID();
    }
    
    
    void getLobbyInfo(steam_id_t lobby_id, lobby_info_t *lobby_info) {
        CSteamID lobbyId, ownerId;
        lobbyId.SetFromUint64(lobby_id);
		
		Sys_DPrintf( "[DUKEMP] Getting lobby info for %s\n", CSTEAM_FormatId( lobby_id ) );
        
        const char *info = SteamMatchmaking()->GetLobbyData(lobbyId, "info");
        if (info[0] != 0) {
            setLobbyInfoFromString(lobby_info, info);
        } else {
            Sys_DPrintf("getLobbyInfo failed: no info assigned with lobby\n");
        }

        lobby_info->id = lobby_id;
        lobby_info->num_players = SteamMatchmaking()->GetNumLobbyMembers(lobbyId);
        
        ownerId = SteamMatchmaking()->GetLobbyOwner(lobbyId);
        
        if (ownerId == CSTEAM_MyID()) {
            Sys_DPrintf("getLobbyInfo: my lobby\n" );
        }

        for (int i = 0; i < lobby_info->num_players; i++) {
            CSteamID memberId = SteamMatchmaking()->GetLobbyMemberByIndex(lobbyId, i);
            memset(lobby_info->players[i].name, 0, sizeof(lobby_info->players[i].name));
            strncpy(lobby_info->players[i].name, SteamFriends()->GetFriendPersonaName(memberId), sizeof(lobby_info->players[i].name)-1) ;
            lobby_info->players[i].id = memberId.ConvertToUint64();
        }
        
        qsort(lobby_info->players, lobby_info->num_players, sizeof(player_info_t), lobby_cmp);
		
		for ( int i = 1; i < lobby_info->num_players; i++ ) {
			if ( lobby_info->players[i].id == ownerId.ConvertToUint64() ) {
				player_info_t tmp;
				memcpy( &tmp, &lobby_info->players[0], sizeof( player_info_t ) );
				memcpy( &lobby_info->players[0], &lobby_info->players[i], sizeof( player_info_t ) ) ;
				memcpy( &lobby_info->players[i], &tmp, sizeof( player_info_t ) );
				break;
			}
		}
        
        for (int i = 0; i < lobby_info->num_players; i++) {
            if (lobby_info->players[i].id == ownerId.ConvertToUint64()) {
                lobby_info->players[i].owner = true;
                lobby_info->owner = &lobby_info->players[i];
            } else {
                lobby_info->players[i].owner = false;
            }
        }
    }
    
    void joinLobby(steam_id_t lobby_id) {
        CSteamID lobbyId;
        lobbyId.SetFromUint64(lobby_id);
        SteamAPICall_t call = SteamMatchmaking()->JoinLobby(lobbyId);
        lobby_join_callback.Set(call, this, &LobbyManager::onLobbyEnter);
    }
    
    void startLobby(steam_id_t lobby_id) {
        SteamMatchmaking()->SetLobbyGameServer(lobby_id, 0, 0, SteamUser()->GetSteamID());
    }
    
    void leaveLobby(steam_id_t lobby_id) {
        SteamMatchmaking()->LeaveLobby(lobby_id);
    }
    
    LobbyManager():handler(0),context(0),
        lobby_chat_update_callback(this, &LobbyManager::onLobbyChatUpdate),
        lobby_data_update_callback(this, &LobbyManager::onLobbyDataUpdate),
        lobby_game_created_callback(this, &LobbyManager::onLobbyGameCreated),
        session_request(this, &LobbyManager::OnP2PSessionRequest),
        join_request(this, &LobbyManager::OnLobbyJoinRequested),
        lobby_chat_message_callback(this, &LobbyManager::onLobbyChatMessage)
    {
    }
    
    void setHandler(CSTEAM_NotificationHandler_t handler, void *context) {
        this->handler = handler;
        this->context = context;
    }
    
    void notifyPong(steam_id_t remote, unsigned int pongval) {
        if (handler) {
            pong_t pong;
            pong.from = remote;
            pong.pongval = pongval;
            pong.failed = 0;
            handler(N_PONG, 0, &pong, context);
        }
    }
};

static LobbyManager *lobbyManager = 0;

char cloudFileNames[MAX_CLOUD_FILES][MAX_CLOUD_FILE_LENGTH] = { 0 };

void dnAddCloudFileNames(void) {
    int count = 0;
    for (int i = 0; i < 10; i++) {
        for (int j=0; j < 4; j++) {
            sprintf(cloudFileNames[count], "game%d_%d.sav", i, j);
            count++;
        }
    }
    strcpy(cloudFileNames[count], "duke3d.cfg");
}



#ifndef NOSTEAM
extern "C"
int CSTEAM_Init(void) {
	int result = SteamAPI_Init();
	if (result) {
        lobbyManager = new LobbyManager();
        workshopManager = new WorkshopManager();
        overlayManager = new OverlayManager();
        CSTEAM_UpdateWorkshopItems();
        CSTEAM_AchievementsInit();
        SteamUtils()->SetOverlayNotificationPosition(k_EPositionTopRight);
        dnAddCloudFileNames();
    }
	return result;
}

extern "C"
void CSTEAM_Shutdown(void) {
	SteamAPI_Shutdown();
}

extern "C"
void CSTEAM_ShowOverlay(const char *dialog) {
    SteamFriends()->ActivateGameOverlay(dialog);
}

extern "C"
int CSTEAM_Online() {
	return (NULL != SteamUserStats() && NULL != SteamUser() && SteamUser()->BLoggedOn());
}

extern "C"
void CSTEAM_AchievementsInit() {
	if(!CSTEAM_Online())
		return;
#ifdef _DEBUG
	SteamUserStats()->ResetAllStats(true);
	SteamUserStats()->StoreStats();
#endif
	SteamUserStats()->RequestCurrentStats();
}

extern "C"
const char * CSTEAM_GetUsername () {
    if (CSTEAM_Online())
        return SteamFriends()->GetPersonaName();
    else return "Duke";
}


extern "C"
int CSTEAM_GetStat(const char * statID) {
	if(!CSTEAM_Online())
		return -1;
	int result = -1;
	SteamUserStats()->GetStat(statID, &result);
	return result;
}

extern "C"
void CSTEAM_SetStat(const char* statID, int number) {
	if(!CSTEAM_Online())
		return;
	SteamUserStats()->SetStat(statID, number);
	SteamUserStats()->StoreStats();
}

extern "C"
void CSTEAM_UnlockAchievement(const char * achievementID) {
    SteamUserStats()->SetAchievement(achievementID);
	SteamUserStats()->StoreStats();
}

extern "C"
void CSTEAM_IndicateProgress(const char * achievementID, int currentNumber, int maxNumber) {
	SteamUserStats()->IndicateAchievementProgress(achievementID, currentNumber, maxNumber);
}

extern "C"
void CSTEAM_DownloadFile(const char * filename) {
	if (!SteamRemoteStorage()->IsCloudEnabledForApp() || !SteamRemoteStorage()->IsCloudEnabledForAccount()) {
		return;
	}

	if (!SteamRemoteStorage()->FileExists(filename)){
		Sys_DPrintf("[CLOUD] file not found in cloud: %s\n", filename);
		return;
	}

	void * buffer = NULL;
	int cubFile = SteamRemoteStorage()->GetFileSize(filename);
	if (cubFile != 0) {
		buffer = malloc(cubFile);
		int result = SteamRemoteStorage()->FileRead(filename, buffer, cubFile);
		if (result) {
			FILE *fp = fopen(filename, "wb+");
			if (fp != NULL) {
				fwrite(buffer, 1, cubFile, fp);
				fclose(fp);
			}
		}
		free(buffer);
	}
}

extern "C"
void CSTEAM_UploadFile(const char * filename) {
	if (!SteamRemoteStorage()->IsCloudEnabledForApp() || !SteamRemoteStorage()->IsCloudEnabledForAccount()) {
		return;
	}
	void * buffer = NULL;
	long length;

	FILE *fp = fopen(filename, "rb");
//	perror(strerror(errno));
	if (fp != NULL){
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		if (length < MAX_CLOUD_FILE_SIZE) {
			fseek (fp, 0, SEEK_SET);
			buffer = malloc(length);
			if (buffer) {
				fread (buffer, 1, length, fp);
			}
			SteamRemoteStorage()->FileWrite(filename, buffer, length);
			free(buffer);
		}
		fclose(fp);
	}
}

extern "C"
void CSTEAM_OpenCummunityHub(void) {
    SteamFriends()->ActivateGameOverlayToWebPage( "http://steamcommunity.com/app/225140" );
}

extern "C"
void CSTEAM_DeleteCloudFile(const char * filename) {
//    if (!SteamRemoteStorage()->IsCloudEnabledForApp() || !SteamRemoteStorage()->IsCloudEnabledForAccount()) {
//        return;
//	}
    SteamRemoteStorage()->FileDelete(filename);
}


/** WORKSHOP STUFF **/
extern "C"
void CSTEAM_UpdateWorkshopItems(){
    if (workshopManager) {
        workshopManager->RefreshSubscribedItems();
    }
}

extern "C"
int32 CSTEAM_NumWorkshopItems() {
    if (workshopManager){
        return workshopManager->GetItemsNumber();
    }
    return 0;
}

extern "C"
void CSTEAM_GetWorkshopItemByIndex(int index, workshop_item_t * item) {
    if (workshopManager){
        *item = workshopManager->GetItemByIndex(index);
    }
}

extern "C"
void CSTEAM_GetWorkshopItemByID(steam_id_t item_id, workshop_item_t * item) {
    if (workshopManager){
        *item = workshopManager->GetItemByID(item_id);
    }
}

extern "C"
void CSTEAM_SubscribeItem(steam_id_t item_id) {
    if (workshopManager){
        workshopManager->Subscribe(item_id);
    }
}

extern "C"
void CSTEAM_UnsubscribeItem(steam_id_t item_id) {
    if (workshopManager){
        workshopManager->Unsubscribe(item_id);
    }
}

/** LOBBY STUFF **/

extern "C"
void CSTEAM_SetNotificationCallback(CSTEAM_NotificationHandler_t handler, void *context) {
    if (lobbyManager) {
        lobbyManager->setHandler(handler, context);
    }
	if (workshopManager) {
		workshopManager->setHandler(handler, context);
	}
	if (overlayManager) {
		overlayManager->setHandler(handler, context);
	}
}

extern "C"
void CSTEAM_CreateLobby(lobby_info_t *lobby_info) {
    if (lobbyManager) {
        lobbyManager->createLobby(lobby_info);
    }
}

extern "C"
void CSTEAM_UpdateLobbyList() {
    if (lobbyManager) {
        lobbyManager->updateLobbyList();
    }
}

extern "C"
int CSTEAM_NumLobbies() {
    return lobbyManager ? lobbyManager->numLobbies() : 0;
}

extern "C"
steam_id_t CSTEAM_GetLobbyId(int index) {
    return SteamMatchmaking()->GetLobbyByIndex(index).ConvertToUint64();
}

extern "C"
int CSTEAM_GetLobbyInfo(steam_id_t lobby_id, lobby_info_t *lobby_info) {
    if (lobbyManager) {
        lobbyManager->getLobbyInfo(lobby_id, lobby_info);
    }
    return SteamMatchmaking()->RequestLobbyData(lobby_id) ? 1 : 0;
}

extern "C"
void CSTEAM_JoinLobby(steam_id_t lobby_id) {
    if (lobbyManager) {
        lobbyManager->joinLobby(lobby_id);
    }
    CSTEAM_DrainQueue();
}

extern "C"
void CSTEAM_SendLobbyMessage(steam_id_t lobby_id, const char *message) {
    SteamMatchmaking()->SendLobbyChatMsg(lobby_id, message, strlen(message));
}

extern "C"
steam_id_t CSTEAM_GameServer() {
    return lobbyManager ? 0 : lobbyManager->game_server.ConvertToUint64();
}

extern "C"
steam_id_t CSTEAM_LobbyOwner(steam_id_t lobby_id) {
    return SteamMatchmaking()->GetLobbyOwner(lobby_id).ConvertToUint64();
}

extern "C"
steam_id_t CSTEAM_OwnLobbyId() {
    return lobbyManager->own_lobby_id.ConvertToUint64();
}

extern "C"
steam_id_t CSTEAM_MyID() {
    return SteamUser()->GetSteamID().ConvertToUint64();
}

extern "C"
void CSTEAM_StartLobby(steam_id_t lobby_id) {
    if (lobbyManager) {
        lobbyManager->startLobby(lobby_id);
    }
}

extern "C"
void CSTEAM_LeaveLobby(steam_id_t lobby_id) {
    if (lobbyManager) {
        lobbyManager->leaveLobby(lobby_id);
    }
}

extern "C"
void CSTEAM_InviteFriends(steam_id_t lobby_id) {
    SteamFriends()->ActivateGameOverlayInviteDialog(lobby_id);
}

extern "C"
void CSTEAM_RunFrame() {
    SteamAPI_RunCallbacks();
}

extern "C"
int  CSTEAM_SendPacket(steam_id_t peer_id, const void *buffer, unsigned int bufsize, int reliable, int channel) {
	return SteamNetworking()->SendP2PPacket( peer_id, buffer, bufsize, reliable ? k_EP2PSendReliable : k_EP2PSendUnreliable, channel);
}

extern "C"
int  CSTEAM_IsPacketAvailable(unsigned int *bufsize, int channel) {
    return SteamNetworking()->IsP2PPacketAvailable(bufsize, channel) ? 1 : 0;
}

extern "C"
int  CSTEAM_ReadPacket(void *buffer, unsigned int bufsize, unsigned int *msgsize, steam_id_t *remote, int channel) {
    CSteamID remote_id;
    int retval = SteamNetworking()->ReadP2PPacket(buffer, bufsize, msgsize, &remote_id, channel) ? 1 : 0;
    *remote = remote_id.ConvertToUint64();
    return retval;
}

extern "C"
void CSTEAM_CloseP2P(steam_id_t remote) {
    SteamNetworking()->CloseP2PSessionWithUser(remote);
}

extern "C"
void CSTEAM_DrainQueue() {
    uint32 msgsize;
    static char buffer[1024];
    CSteamID remote;
    int c = 0;
    
    while (SteamNetworking()->IsP2PPacketAvailable(&msgsize)) {
        SteamNetworking()->ReadP2PPacket(&buffer[0], 1024, &msgsize, &remote);
        c++;
        if (buffer[0] == 254) {
            Sys_DPrintf("DrainQueue: drained '254'\n");
        }
    }
    
    Sys_DPrintf("DrainQueue: drained %d packets\n", c);
}

extern "C"
const char* CSTEAM_FormatId(steam_id_t steam_id) {
    static char buffer[50];
    CSteamID steamId;
    steamId.SetFromUint64(steam_id);
    sprintf(buffer, "STEAM_0:%u:%u", (steamId.GetAccountID() % 2) ? 1 : 0, (unsigned int)steamId.GetAccountID()/2);
    return buffer;
}

extern "C"
int CSTEAM_ShowGamepadTextInput (const char * defaultText, int maxChars) {
    if (SteamUtils()->ShowGamepadTextInput(k_EGamepadTextInputModeNormal, k_EGamepadTextInputLineModeSingleLine, "", maxChars, defaultText)) {
        return 1;
    } else {
        return 0;
    }
}

extern "C"
int CSTEAM_GetEnteredGamepadTextLength () {
    return SteamUtils()->GetEnteredGamepadTextLength();
}

extern "C"
int CSTEAM_GetEnteredGamepadTextInput (char *pchText, int cchText) {
    return SteamUtils()->GetEnteredGamepadTextInput(pchText, cchText);
}

extern "C"
void CSTEAM_SetPlayedWith(steam_id_t playerid) {
    SteamFriends()->SetPlayedWith(playerid);
}


#else

extern "C"
int CSTEAM_Init(void) {
	printf("*** NOSTEAM Initialization ***\n");
    dnAddCloudFileNames();
    return 1;
}

extern "C"
void CSTEAM_Shutdown(void) {
	printf("*** NOSTEAM Shutdown ***\n");
}

extern "C"
void CSTEAM_ShowOverlay(const char *dialog) {
    printf("*** NOSTEAM ShowOverlay ***\n");
}

extern "C"
int CSTEAM_Online() {
	printf("*** NOSTEAM Online ***\n");
	return 1;
}

extern "C"
void CSTEAM_AchievementsInit() {
	printf("*** NOSTEAM AchievementsInit ***\n");
}

extern "C"
int CSTEAM_GetStat(const char * statID) {
	printf("*** NOSTEAM GetStat ***\n");
	return 0;
}

extern "C"
const char * CSTEAM_GetUsername () {
    printf("*** NOSTEAM GetUsername ***\n");
    return "NoSteam";
}

extern "C"
void CSTEAM_SetStat(const char* statID, int number) {
	printf("*** NOSTEAM SetStat ***\n");
}

extern "C"
void CSTEAM_UnlockAchievement(const char * achievementID) {
   printf("*** NOSTEAM UnlockAchievement ***\n");
}

extern "C"
void CSTEAM_IndicateProgress(const char * achievementID, int currentNumber, int maxNumber) {
	printf("*** NOSTEAM IndicateProgress ***\n");
}

extern "C"
void CSTEAM_DownloadFile(const char * filename) {
}

extern "C"
void CSTEAM_UploadFile(const char * filename) {
};


/** WORKSHOP STUFF **/
extern "C"
void CSTEAM_UpdateWorkshopItems(){
}

extern "C"
int32 CSTEAM_NumWorkshopItems() {
    return 0;
}

extern "C"
void CSTEAM_GetWorkshopItemByIndex(int index, workshop_item_t * item) {
}

extern "C"
void CSTEAM_GetWorkshopItemByID(steam_id_t item_id, workshop_item_t * item) {
}

extern "C"
void CSTEAM_SubscribeItem(steam_id_t item_id) {
}

extern "C"
void CSTEAM_UnsubscribeItem(steam_id_t item_id) {
}

extern "C"
void CSTEAM_OpenCummunityHub(void) {
    printf("*** NOSTEAM OpenCummunityHub ***\n");
}

extern "C"
void CSTEAM_DeleteCloudFile(const char * filename) {
    printf("*** NOSTEAM DeleteCloudFile ***\n");
}

extern "C"
void CSTEAM_SetNotificationCallback(CSTEAM_NotificationHandler_t handler, void *context) {
    
}

extern "C"
void CSTEAM_CreateLobby(lobby_info_t *lobby_info) {
}

extern "C"
void CSTEAM_UpdateLobbyList() {
}

extern "C"
int CSTEAM_NumLobbies() {
    return 0;
}

extern "C"
steam_id_t CSTEAM_GetLobbyId(int index) {
    return 0;
}

extern "C"
int CSTEAM_GetLobbyInfo(steam_id_t lobby_id, lobby_info_t *lobby_info) {
    return 0;
}

extern "C"
void CSTEAM_JoinLobby(steam_id_t lobby_id) {

}

extern "C"
void CSTEAM_SendLobbyMessage(steam_id_t lobby_id, const char *message) {
}

extern "C"
int  CSTEAM_SendReliablePacket(steam_id_t peer_id, const void *buffer, unsigned int bufsize) {
    return 0;
}

extern "C"
int  CSTEAM_SendUnreliablePacket(steam_id_t peer_id, const void *buffer, unsigned int bufsize) {
    return 0;
}


extern "C"
steam_id_t CSTEAM_GameServer() {
    return 0;
}

extern "C"
steam_id_t CSTEAM_LobbyOwner(steam_id_t lobby_id) {
    return 0;
}

extern "C"
steam_id_t CSTEAM_OwnLobbyId() {
    return 0;
}

extern "C"
steam_id_t CSTEAM_MyID() {
    return 0;
}

extern "C"
void CSTEAM_StartLobby(steam_id_t lobby_id) {
}

extern "C"
void CSTEAM_LeaveLobby(steam_id_t lobby_id) {
}

extern "C"
void CSTEAM_InviteFriends(steam_id_t lobby_id) {
}

extern "C"
void CSTEAM_RunFrame() {
    
}

extern "C"
int  CSTEAM_SendPacket(steam_id_t peer_id, const void *buffer, unsigned int bufsize, int reliable, int channel) {
    return 0;
}

extern "C"
int  CSTEAM_IsPacketAvailable(unsigned int *bufsize, int channel) {
    return 0;
}

extern "C"
int  CSTEAM_ReadPacket(void *buffer, unsigned int bufsize, unsigned int *msgsize, steam_id_t *remote, int channel) {
    return 0;
}

extern "C"
void CSTEAM_CloseP2P(steam_id_t remote) {
}

extern "C"
void CSTEAM_DrainQueue() {
}

extern "C"
void CSTEAM_CheckPing() {
}

extern "C"
unsigned int CSTEAM_Ping(steam_id_t remote) {
    return 0;
}

extern "C"
const char* CSTEAM_FormatId(steam_id_t steam_id) {
    return 0;
}

extern "C"
void CSTEAM_SetPlayedWith(steam_id_t playerid) {
}


#endif
