//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "gui_private.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <SDL.h>

#include <Rocket/Core.h>
#include <Rocket/Core/SystemInterface.h>
#include <Rocket/Core/FontDatabase.h>
#include <Rocket/Debugger.h>
#include <Rocket/Controls.h>

#include "ShellSystemInterface.h"
#include "ShellRenderInterfaceOpenGL.h"
#include "ShellFileInterface.h"
#include "ShellOpenGL.h"
#include "FrameAnimationDecoratorInstancer.h"
#include "helpers.h"
#include "csteam.h"
#include "dnAPI.h"
#include "dnMulti.h"
#include "dnMouseInput.h"
//#include "duke3d.h"
//#include "glguard.h"

extern "C" {
#include "types.h"
#include "gamedefs.h"
#include "function.h"
#include "config.h"
#include "build.h"
#include "duke3d.h"
#include "mmulti.h"
}

#ifndef _WIN32
#include <unistd.h>
#endif

#include "crc32.h"

#define NUM_PING_ATTEMPTS 10
#define MAX_FAILED_ATTEMPTS 3
#define MAX_PING 180


extern "C" {
void Sys_DPrintf(const char *format, ...);
double Sys_GetTicks();
extern int32_t r_usenewshading;
extern int32_t r_usetileshades;
extern int AccurateLighting;
extern SDL_GameController *gamepad;
}

void encode(std::string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    data.swap(buffer);
}

#ifdef _WIN32
char* stpcpy(char *s, const char *v) {
	strcpy(s, v);
	return s + strlen(v);
}
#endif

static
const char* rml_escape( const char *string ) {
	static char buffer[65536];
	char *ptr = buffer;
	*ptr = 0;
	
	for ( const char *inchar = string; *inchar != 0; inchar++ ) {
		switch ( *inchar ) {
			case '&':  ptr = stpcpy(ptr, "&amp;");   break;
            case '\"': ptr = stpcpy(ptr, "&quot;");  break;
            case '\'': ptr = stpcpy(ptr, "&apos;");  break;
            case '<':  ptr = stpcpy(ptr, "&lt;");    break;
            case '>':  ptr = stpcpy(ptr, "&gt;");    break;
            default:   *ptr = *inchar; ptr++; *ptr = 0; break;
		}
	}
	
	return buffer;
}

static
void doGameLoop() {
	handleevents();
	getpackets();
	menus();
	nextpage();
	menus();
}

static
void NotificationCallback(CSTEAM_Notification_t notification, int status, void *data, void *context) {
    GUI *gui = (GUI*)context;
    if (status == 0) {
        switch (notification) {
            case N_LOBBY_CREATED: {
                gui->OnLobbyCreated((lobby_info_t*)data);
                break;
            }
            case N_LOBBY_MATCH_LIST: {
                gui->OnLobbyListUpdated();
                break;
            }
            case N_LOBBY_ENTER: {
                gui->OnLobbyEnter((lobby_info_t*)data);
                break;
            }
            case N_LOBBY_CHAT_UPDATE: {
                gui->OnLobbyMembersChanged((lobby_info_t*)data);
                break;
            }
            case N_LOBBY_CHAT_MSG: {
                gui->OnLobbyMessage((lobby_message_t*)data);
                break;
            }
            case N_LOBBY_GAME_CREATED: {
                gui->OnLobbyStarted((lobby_info_t*)data);
                break;
            }
            case N_LOBBY_DATA_UPDATE: {
                gui->OnLobbyDataUpdate((lobby_info_t*)data, status);
                break;
            }
            case N_LOBBY_INVITE: {
                gui->OnLobbyJoinInvite(((lobby_info_t*)data)->id);
                break;
            }
			case N_WORKSHOP_UPDATE: {
				gui->InitUserMapsPage("menu-usermaps");
				break;
			}
			case N_WORKSHOP_SUBSCRIBED: {
				gui->OnWorkshopSubscribe((workshop_item_t*)data, status);
                break;
            }
			default: {
				break;
            }
        }
    } else {
        switch (notification) {
			case N_LOBBY_DATA_UPDATE: {
                gui->OnLobbyDataUpdate((lobby_info_t*)data, status);
                break;
            }
			case N_WORKSHOP_SUBSCRIBED: {
				gui->OnWorkshopSubscribe((workshop_item_t*)data, status);
                break;
			}
            case N_BIGPICTURE_GAMEPAD_TEXT_UPDATE: {
                gui->OnBigPictureTextUpdate((const char*)data, status);
                break;
            }
			default:
				break;
		}
	}
}

class MessageBoxManager {
	
private:
	
	Rocket::Core::Context *m_context;
	RocketMenuPlugin *m_menu;
	bool m_shown;
	int m_status;
	
	enum {
		CMD_CLOSE = 999
	};
	
public:
	
	MessageBoxManager( Rocket::Core::Context *context, RocketMenuPlugin *menu ):
		m_context( context ),
		m_menu( menu ),
		m_shown( false ),
		m_status( 0 )
	{
		
	}
	
	virtual ~MessageBoxManager() {
		
	}
	
	void showMessageBox( const char *text, ... ) {
		m_shown = true;
		m_status = -1;
		
		Rocket::Core::ElementDocument *doc = m_context->GetDocument( "messagebox" );
		
		Rocket::Core::Element *queston = doc->GetElementById( "messagebox-text" );
		queston->SetInnerRML( rml_escape( text ) );
		
		Rocket::Core::Element *menu = doc->GetElementById( "messagebox-menu" );
		menu->SetInnerRML( " " );
		
		va_list ap;
		va_start( ap, text );
		
		/* populate menu options */
		int index = 0;
		for ( const char *s = va_arg( ap, const char* ); s != NULL; s = va_arg( ap, const char* ), index++ ) {
			Rocket::Core::Element *element = new Rocket::Core::Element("div");
			element->SetId( va( "messagebox-%d", index ) );
			element->SetAttribute( "command" , va( "messagebox-%d", index ) );
			element->SetInnerRML( rml_escape( s ) );
			menu->AppendChild( element );
			m_menu->SetupMenuItem( element );
		}
		
		if ( index == 0 ) {
			/* add dummy invisible option */
			Rocket::Core::Element *element = new Rocket::Core::Element("div");
			element->SetId( va( "messagebox-dummy" ) );
			element->SetInnerRML( " " );
			element->SetProperty( "display", "none" );
			menu->AppendChild( element );
			m_menu->SetupMenuItem( element );
		}
		
		va_end( ap );
		
		m_menu->ShowModalDocument( m_context, "messagebox" );
	}
	
	int wait() {
		do {
			doGameLoop();
		} while ( isShown() );
		return getStatus();
	}
	
	bool isShown() {
		return m_shown;
	}
	
	int getStatus() {
		return m_status;
	}
	
	void close() {
		m_menu->HideModalDocument( m_context );
		m_shown = false;
	}
	
	bool doCommand( Rocket::Core::Element *element, const Rocket::Core::String& command ) {
		int status = -1;
		if ( sscanf( command.CString(), "messagebox-%d", &status ) == 1 ) {
			if ( m_status == -1 ) {
				m_status = status;
			}
			if ( status != CMD_CLOSE ) {
				close();
			}
			m_shown = false;
			return true;
		}
		return false;
	}
};

class LobbyInfoRequestManager {
	
private:
	
	double m_requestStartTime;
	double m_lastRequestTime;
	bool m_complete;
	bool m_success;
	steam_id_t m_lobby_id;
	lobby_info_t m_lobbyInfo;
	
public:
	
	LobbyInfoRequestManager() :
		m_complete( false ),
		m_success( false )
	{
	}
	
	virtual ~LobbyInfoRequestManager() { }
	
	bool requestLobbyInfo( steam_id_t lobby_id ) {
		m_complete = false;
		m_success = false;
		m_lobby_id = lobby_id;
		m_lastRequestTime = m_requestStartTime = Sys_GetTicks();
		memset( &m_lobbyInfo, 0, sizeof( m_lobbyInfo ) );
		return (bool)CSTEAM_GetLobbyInfo( m_lobby_id, &m_lobbyInfo );
	}
	
	bool isComplete() {
		double now = Sys_GetTicks();
		
		if ( now - m_lastRequestTime > 2000.0 ) {
			m_lastRequestTime = now;
			CSTEAM_GetLobbyInfo( m_lobby_id, &m_lobbyInfo );
		}
		
		if ( m_lobbyInfo.server != 0 && m_lobbyInfo.version != 0 ) {
			m_complete = true;
			m_success = true;
		}
		return m_complete;
	}
	
	bool succeeded() {
		return m_success;
	}
	
	lobby_info_t* getLobbyInfo() {
		if ( m_complete && m_success ) {
			return &m_lobbyInfo;
		}
		return NULL;
	}
	
	bool isTimedOut() {
		return Sys_GetTicks() - m_requestStartTime > 10000.0;
	}
	
	void onLobbyDataUpdate( lobby_info_t *lobby_info, int status ) {
		if ( status == 0 ) {
			m_success = true;
			m_complete = true;
			memcpy( &m_lobbyInfo, lobby_info, sizeof( m_lobbyInfo ) );
		} else {
			m_success = false;
			m_complete = true;
		}
	}
};

class WorkshopRequestManager {
private:
	steam_id_t m_itemId;
	double m_requestStartTime;
	bool m_success, m_complete;
public:
	WorkshopRequestManager():
		m_itemId( 0 ),
		m_requestStartTime( 0 )
	{
		
	}
	
	virtual ~WorkshopRequestManager() {	}
	
	void subscribeItem( steam_id_t itemId ) {
		m_itemId = itemId;
		CSTEAM_SubscribeItem( m_itemId );
	}
	
	bool isComplete() {
		return m_complete;
	}
	
	bool succeeded() {
		return m_success;
	}
	
	void onWorkshopSubscribe(workshop_item_t *item, int status) {
		m_complete = true;
		m_success = status == 0;
	}
};

struct ConfirmableAction {
    Rocket::Core::ElementDocument *doc;
    Rocket::Core::ElementDocument *back_page;
    virtual void Yes() {};
    virtual void No() {};
    virtual void OnClose() { };
    ConfirmableAction(Rocket::Core::ElementDocument* back_page):back_page(back_page){}
    virtual ~ConfirmableAction(){}
};

static
Rocket::Core::String KeyDisplayName(const Rocket::Core::String& key_id) {
    if (key_id.Length() == 0) {
        return Rocket::Core::String("None");
    }
    if (key_id[0] == '$') {
        int n;
        char name[20];
        if (sscanf(key_id.CString(), "$mouse%d", &n) == 1) {
            sprintf(name, "Mouse %d", n);
            return Rocket::Core::String(name);
        }
    }
    return key_id;
}

static const Rocket::Core::Vector2i menu_virtual_size(1920, 1080);

static
void LoadFonts(const char* directory)
{
	Rocket::Core::String font_names[1];
	font_names[0] = "dukefont.ttf";

	for (int i = 0; i < sizeof(font_names) / sizeof(Rocket::Core::String); i++)	{
		Rocket::Core::FontDatabase::LoadFontFace(Rocket::Core::String(directory) + font_names[i]);
	}
}

void GUI::LoadDocuments() {
    Rocket::Core::ElementDocument *cursor = m_context->LoadMouseCursor("data/pointer.rml");
    
    if (cursor != NULL) {
        cursor->RemoveReference();
    }

	LoadDocument("data/menubg.rml");
	LoadDocument("data/start.rml");
	LoadDocument("data/mainmenu.rml");
    LoadDocument("data/ingamemenu.rml");
	LoadDocument("data/options.rml");
    LoadDocument("data/credits.rml");
    LoadDocument("data/loadgame.rml");
    LoadDocument("data/savegame.rml");
	LoadDocument("data/skill.rml");
	LoadDocument("data/episodes.rml");
	LoadDocument("data/credits.rml");
	LoadDocument("data/video.rml");
	LoadDocument("data/game.rml");
	LoadDocument("data/sound.rml");
    LoadDocument("data/mouse.rml");
    LoadDocument("data/gamepad.rml");
	LoadDocument("data/keys.rml");
    LoadDocument("data/keyprompt.rml");
    LoadDocument("data/yesno.rml");
    LoadDocument("data/videoconfirm.rml");
    LoadDocument("data/usermaps.rml");
    LoadDocument("data/multiplayer.rml");
    LoadDocument("data/joingame.rml");
    LoadDocument("data/creategame.rml");
    LoadDocument("data/lobby.rml");
    LoadDocument("data/help.rml");
    LoadDocument("data/quitconfirm.rml");
    LoadDocument("data/mpmaps.rml");
    LoadDocument("data/ok.rml");
	LoadDocument("data/messagebox.rml");
    LoadDocument("data/waiting.rml");
    LoadDocument("data/filters.rml");
    LoadDocument("data/mpinfo.rml");
}

void GUI::GetAddonDocumentPath(int addon, const Rocket::Core::String& path, char * addonPath) {
    size_t dotPos = path.Find(".");
    const Rocket::Core::String& docName = path.Substring(0, dotPos);
    const Rocket::Core::String& extension = path.Substring(dotPos+1, path.Length() - docName.Length());
    sprintf(addonPath, "%s_%d.%s", docName.CString(), addon, extension.CString());
}


void GUI::LoadDocument(const Rocket::Core::String& path) {
#ifndef _WIN32
//	char data[1024];
//    getcwd(data, 1024);
//    printf("working dir: %s\n",data);
#endif
    Rocket::Core::ElementDocument *document = 0;
    
    int addonId = dnGetAddonId();
    if (addonId != 0) {
        char addonPath[1024];
        GUI::GetAddonDocumentPath(addonId, path, addonPath);
        document = m_context->LoadDocument(addonPath);
    }
    
    if (document == 0) {
        document = m_context->LoadDocument(path);
    }

	assert(document != 0);
	document->RemoveReference();
}

void GUI::Reload(){
	m_context->UnloadAllDocuments();
	LoadDocuments();
	m_menu->ShowDocument(m_context, "menu-keys-setup");
}

void GUI::SetActionToConfirm(ConfirmableAction *action) {
    if (m_action_to_confirm != NULL) {
        delete m_action_to_confirm;
    }
    m_action_to_confirm = action;
    m_draw_strips = m_action_to_confirm != NULL;
}

GUI::GUI(int width, int height):m_enabled(false),m_width(width),m_height(height),m_enabled_for_current_frame(false),m_waiting_for_key(false),m_action_to_confirm(NULL),m_need_apply_video_mode(false),m_need_apply_vsync(false),m_show_press_enter(true),m_draw_strips(false),m_last_poll(0),m_join_on_launch(false) {
    
	
    for (int i = 1; i < _buildargc-1; i++) {
        if (!strcmp(_buildargv[i], "+connect_lobby")) {
            steam_id_t lobby_id;
            if (sscanf(_buildargv[i+1], "%llu", &lobby_id) == 1) {
                m_join_on_launch = true;
				m_invited_lobby = lobby_id;
            }
            break;
        }
    }
    
    m_last_poll = Sys_GetTicks();
	m_systemInterface = new ShellSystemInterface();
	m_renderInterface = new ShellRenderInterfaceOpenGL();

	m_menu = new RocketMenuPlugin();
	m_menu->SetDelegate(this);

	m_animation = new RocketAnimationPlugin();
	m_menu->SetAnimationPlugin(m_animation);

	Rocket::Core::SetSystemInterface(m_systemInterface);
	Rocket::Core::SetRenderInterface(m_renderInterface);
	Rocket::Core::RegisterPlugin(m_menu);
	Rocket::Core::RegisterPlugin(m_animation);
	Rocket::Core::Initialise();

	Rocket::Core::DecoratorInstancer *decorator_instancer = new FrameAnimationDecoratorInstancer();
	Rocket::Core::Factory::RegisterDecoratorInstancer("frame-animation", decorator_instancer);
	Rocket::Controls::Initialise()	;

	//Rocket::Core::Factory::RegisterFontEffectInstancer();

	decorator_instancer->RemoveReference();

	LoadFonts("data/assets/");

	m_context = Rocket::Core::CreateContext("menu", menu_virtual_size);
	assert(m_context != 0);

#if USE_ROCKET_DEBUGGER
	Rocket::Debugger::Initialise(m_context);
#endif

	UpdateMenuTransform();

	LoadDocuments();
	m_menu->ShowDocument(m_context, "menu-start");
	SetupAnimation();
	m_context->ShowMouseCursor(false);
    
    char version_string[100];
    sprintf(version_string, "Version: %s", dnGetVersion());
    
    Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-main");
    Rocket::Core::Element *e = doc->GetElementById("version-footer");
    e->SetInnerRML(version_string);
    
    menu_to_open = "menu-ingame";
    CSTEAM_SetNotificationCallback(NotificationCallback, this);

	m_messageBoxManager = new MessageBoxManager(m_context, m_menu);
	m_lobbyInfoRequestManager = new LobbyInfoRequestManager();
	m_workshopRequestManager = new WorkshopRequestManager();
	
    exittotitle = 1;
    m_setBrightness = -1;
    m_lastBrightnessUpdate = Sys_GetTicks();
}

/*
static
void ColorAnimation(Rocket::Core::Element *e, float position, void *ctx) {
	rgb p(235, 156, 9};
	rgb q(255, 255, 255);
	rgb v = rgb_lerp(p, q, position);

	char color[50];
	int r = (int)(v.r*255);
	int g = (int)(v.g*255);
	int b = (int)(v.b*255);
	sprintf(color, "rgb(%d,%d,%d)", r, g, b);
	e->SetProperty("color", color);;
}
*/

void GUI::SetupAnimation() {
//	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-main");
//	Rocket::Core::Element *e = doc->GetElementById("new-game");
//	m_animation->AnimateElement(e, AnimationTypeBounce, 0.3f, ColorAnimation, NULL);
}

GUI::~GUI() {
	delete m_workshopRequestManager;
	delete m_lobbyInfoRequestManager;
	delete m_messageBoxManager;
	delete m_context;
	Rocket::Core::Shutdown();
	delete m_animation;
	delete m_menu;
	delete m_systemInterface;
	delete m_renderInterface;
}

void GUI::PreModeChange() {
	m_renderInterface->GrabTextures();
}

void GUI::PostModeChange(int width, int height) {
	m_width = width;
	m_height = height;
	m_renderInterface->RestoreTextures();
	UpdateMenuTransform();
}

void GUI::UpdateMenuTransform() {
    int eff_width = m_width;
    int eff_height = m_height;
	m_menu_offset_y = 0;

    if (m_width == 1920 && m_height == 1200) {
        eff_height = 1080;
		m_menu_offset_y = (1200-1080)/2;
    }

	float aspect = (float)eff_width/(float)eff_height;
	m_menu_scale = (float)eff_height/(float)menu_virtual_size.y;
	m_menu_offset_x = ((menu_virtual_size.y*aspect) - menu_virtual_size.x)/2.0f;
	m_renderInterface->SetTransform(m_menu_scale, m_menu_offset_x, m_menu_offset_y, eff_height);
}

static const float P_Q = 1.0f; /* perspective coeff */

static void
GL_SetUIMode(int width, int height, float menu_scale, float menu_offset_x, float menu_offset_y) {
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glScalef(menu_scale, menu_scale, 1.0f);
	glTranslatef(menu_offset_x, menu_offset_y, 0);
}

static void DrawStrips() {
    glEnable(GL_COLOR);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
    glColor4ub(0, 0, 0, 220);
    
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2d(0, 0);
    glVertex2d(1920, 0);
    glVertex2d(0, -60);
    glVertex2d(1920, -60);
    glEnd();

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2d(0, 1140);
    glVertex2d(1920, 1140);
    glVertex2d(0, 1080);
    glVertex2d(1920, 1080);
    glEnd();
}

static unsigned long
GetMapHash(const char *mapname) {
    unsigned long result = 0;
    FILE *f = fopen(va("%s/%s", defaultmapspath, mapname), "rb");
    if (f != NULL) {
        crc32init(&result);
        crc32file(f, &result);
        crc32finish(&result);
        fclose(f);
    }
    return result;
}


extern "C" {
    void Sys_DPrintf(const char *format, ...);
    void Sys_Restart(const char *options);
    unsigned long getgrpsig();
}

void GUI::FillLobbyInfo(lobby_info_t *lobby_info) {
    Rocket::Core::ElementDocument *menu_page = m_context->GetDocument("menu-multiplayer-create");
	
	lobby_info->version = MPVERSION;
	lobby_info->session_token = rand();
    GetOptionNumericValue(menu_page, "gamemode", "mode-", &lobby_info->mode);
    GetOptionNumericValue(menu_page, "maxplayers", "maxp-", &lobby_info->maxplayers);
    GetOptionNumericValue(menu_page, "fraglimit", "fraglimit-", &lobby_info->fraglimit);
    GetOptionNumericValue(menu_page, "timelimit", "timelimit-", &lobby_info->timelimit);
    GetOptionNumericValue(menu_page, "skill", "skill-", &lobby_info->monster_skill);
    if (m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "friendlyfire"))->GetId() == "ff-enabled") {
        lobby_info->friendly_fire = 1;
    } else {
        lobby_info->friendly_fire = 0;
    }
    if (m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "private"))->GetId() == "private-on") {
        lobby_info->private_ = 1;
    } else {
        lobby_info->private_ = 0;
    }
    
    if (m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "markers"))->GetId() == "markers-on") {
        lobby_info->markers = 1;
    } else {
        lobby_info->markers = 0;
    }
    
    lobby_info->addon = dnGetAddonId();

    if (lobby_info->mode == 2) { //coop
        lobby_info->fraglimit = 0;
        lobby_info->timelimit = 0;
    } else {
        lobby_info->friendly_fire = 1;
        lobby_info->monster_skill = 0;
    }

    Rocket::Core::Element *level = menu_page->GetElementById("mp-level");
    Rocket::Core::String mapname = level->GetAttribute("mapname")->Get<Rocket::Core::String>();
    if (mapname.Length() < sizeof(lobby_info->mapname)) {
        strcpy(lobby_info->mapname, mapname.CString());
    } else {
        strcpy(lobby_info->mapname, "<N/A>"); /* TODO: handle wrong map name */
    }
    lobby_info->grpsig = getgrpsig();
    lobby_info->usermap = level->IsClassSet("usermap") ? 1 : 0;
    if (lobby_info->usermap) {
        lobby_info->maphash = GetMapHash(lobby_info->mapname);
    } else {
        lobby_info->maphash = 0;
    }
    
    if (level->IsClassSet("workshop")) {
        steam_id_t item_id;
        Rocket::Core::String item_id_string = level->GetAttribute("item-id")->Get<Rocket::Core::String>();
        sscanf(item_id_string.CString(), "%llu", &item_id);
        lobby_info->workshop_item_id = item_id;
    }
    
    char temp[1024];
    strcpy(temp, dnFilterUsername(CSTEAM_GetUsername()));
    if (strlen(temp) > 11) {
        temp[11] = '\0';
    }
    sprintf(lobby_info->name, "%s's lobby", temp);
}

void GUI::CreateLobby() {
    lobby_info_t lobby_info = {0};
    
    FillLobbyInfo(&lobby_info);
    
	netcleanup();
	
    CSTEAM_CreateLobby(&lobby_info);
    
    m_menu->ShowDocument(m_context, "menu-multiplayer-lobby");
    UpdateLobbyPage(&lobby_info);

    Sys_DPrintf("*** Creating Lobby ***\n");
    Sys_DPrintf("My SteamID: %s\n", CSTEAM_FormatId(CSTEAM_MyID()));
    
}

void GUI::ShowErrorMessage(const char *page_id, const char *message) {
    struct LobbyErrorMessage: public ConfirmableAction {
        Rocket::Core::Context *context;
        RocketMenuPlugin *menu;
        LobbyErrorMessage(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu):ConfirmableAction(back_page),context(context), menu(menu) {}
        virtual void Yes() {
        }
    };
    Rocket::Core::ElementDocument *page = m_context->GetDocument(page_id);

    ShowConfirmation(new LobbyErrorMessage(page, m_context, m_menu), "ok", message);
}

bool GUI::VerifyLobby(lobby_info_t *lobby_info) {
    
    if ( lobby_info->version == 0 ) {
        return false;
    }
    
    if ( lobby_info->version != MPVERSION ) {
		Sys_DPrintf( "[DUKEMP] Wrong server version: %d (mine is %d)\n", lobby_info->version, MPVERSION );
		m_messageBoxManager->showMessageBox( "Wrong server version", "Close", NULL );
		m_messageBoxManager->wait();
        return false;
    }
    
    if ( lobby_info->grpsig != getgrpsig() ) {
		Sys_DPrintf( "[DUKEMP] GRP signature mismatch\n" );
		m_messageBoxManager->showMessageBox( "GRP signature mismatch", "Close", NULL );
		m_messageBoxManager->wait();
        return false;
    }
    
    if ( lobby_info->workshop_item_id == 0 &&
		 lobby_info->usermap &&
		 lobby_info->maphash != GetMapHash( lobby_info->mapname) )
	{
		Sys_DPrintf( "[DUKEMP] Custom map hash mismatch\n" );
		m_messageBoxManager->showMessageBox( "Custom map hash mismatch", "Close", NULL );
		m_messageBoxManager->wait();
        return false;
    }
	
    return true;
}

void GUI::JoinLobby(steam_id_t lobby_id) {
	double lastPingTime;
	bool pingSent = false;
	int pingval = 0;
	int succeededAttempts = 0;
	int failedAttempts = 0;
	bool firstAttempt = true;
	double latencySum = 0;
	lobby_info_t *li;
	
	m_messageBoxManager->showMessageBox( "Retrieving lobby info...", "Cancel", NULL );
	m_lobbyInfoRequestManager->requestLobbyInfo( lobby_id );
	    
	netcleanup();
	
	Sys_DPrintf( "[DUKEMP] starting lobby join (%llu)\n", lobby_id );
	
	// Measure network quality.
	// Measurement is done by sending sequence of 'ping' requests.
	// We skip the very first request because initial data exchange is
	// usually slow: steam runtime establishes direct connection between
	// peers. Subsequent ping requests are done to measure network latency.
	// We stop this process if the number of failed ping requests
	// exceeds certain amount (MAX_FAILED_ATTEMPTS), this means that
	// network doesnt work well enough. Otherwise we calculate average
	// latency and then, if this value exceeds MAX_PING, say that
	// the connection is too laggy. If the average latency is good
	// we proceed with verifying the lobby. Lobby verification consists of
	// checking GRP checksum, usermap checksum. If the lobby has custom
	// workshop map, we sign up for this map and then wait when steam
	// downloads it. And finally we're ready to enter the lobby.
	
	// latency check
	do {
		doGameLoop(); // true means that we skip 'getpackets', it would interfere with CSTEAM_CheckPong
		if ( m_lobbyInfoRequestManager->isComplete() ) {
			double timeout;
			li = m_lobbyInfoRequestManager->getLobbyInfo();
            
            if (li->addon != dnGetAddonId()) {
                Sys_Restart(va("-addon %d +connect_lobby %llu", li->addon, lobby_id));
            }
            
			if ( !pingSent ) {
				CSTEAM_CloseP2P( li->server );
				pingval = dnPing( li->server );
				lastPingTime = Sys_GetTicks();
				pingSent = true;
				firstAttempt = true;
				timeout = MAX_PING*30;
			}
			if ( pingSent ) {
				double now = Sys_GetTicks();
				if ( dnCheckPong( li->server, pingval ) ) {

					Sys_DPrintf( "[DUKEMP] got pong\n" );
					timeout = MAX_PING;
					if ( !firstAttempt ) { // skip the very first attempt
						succeededAttempts++;
						double latency = now - lastPingTime;
						latencySum += latency;
					} else {
						firstAttempt = false;
					}
					
					pingval = dnPing( li->server );
					lastPingTime = Sys_GetTicks();
				}
				if ( now - lastPingTime > timeout ) {
					Sys_DPrintf( "[DUKEMP] ping timed out\n" );
					
					failedAttempts++; // ping request failed
					
					pingval = dnPing( li->server );
					lastPingTime = now;
				}
			}
		}
	} while (
				 m_messageBoxManager->isShown() &&			 
				 failedAttempts + succeededAttempts < NUM_PING_ATTEMPTS &&
				 failedAttempts < MAX_FAILED_ATTEMPTS
			 );
	
	double avgLatency = latencySum/succeededAttempts;
	
	Sys_DPrintf( "[DUKEMP] cancelled: %s\n", m_messageBoxManager->isShown() ? "no" : "yes" );
	Sys_DPrintf( "[DUKEMP] failed attempts: %d\n", failedAttempts );
	Sys_DPrintf( "[DUKEMP] succeded attempts: %d\n", succeededAttempts );
	Sys_DPrintf( "[DUKEMP] avg latency: %g\n", avgLatency );
	
	if ( m_messageBoxManager->isShown() ) {
		m_messageBoxManager->close();
		
		if ( failedAttempts >= MAX_FAILED_ATTEMPTS ) {
			Sys_DPrintf( "[DUKEMP] Network connection is not stable enough (too many packet drops)\n" );
			if ( succeededAttempts == 0 ) {
				m_messageBoxManager->showMessageBox( "Timed out", "Close", NULL );
			} else {
				m_messageBoxManager->showMessageBox( "Unstable connection", "Close", NULL );
			}
			m_messageBoxManager->wait();
			return;
		}
		
		if ( avgLatency > MAX_PING ) {
			const char *ignore = m_invited_lobby == lobby_id ? "Ignore" : NULL;
			m_messageBoxManager->showMessageBox( "Network latency is too " /*"damn "*/ "high", "Close", ignore, NULL );
			if ( m_messageBoxManager->wait() != 1 ) {
				return;
			}
		}
		
		if ( !VerifyLobby( li ) ) {
			Sys_DPrintf( "[DUKEMP] the lobby isn't suitable, no connect\n" );
			return;
		}
		
		if ( li->workshop_item_id != 0 ){ // the lobby is running an workshop map, download it if needed
			workshop_item_t item = { 0 };
			CSTEAM_GetWorkshopItemByID( li->workshop_item_id, &item );
			if ( item.item_id == 0 ) { // not subscribed
				m_workshopRequestManager->subscribeItem( li->workshop_item_id );
				m_messageBoxManager->showMessageBox( "Downloading map...", "Cancel", NULL );
				
				bool mapDownloaded = false;
				double lastCheckTime = 0.0;
				
				do {
					doGameLoop();
					if ( m_workshopRequestManager->isComplete() ) {
						double now = Sys_GetTicks();
						if ( now - lastCheckTime > 500.0 ) { // poll workshop item twice per second
							lastCheckTime = now;
							CSTEAM_GetWorkshopItemByID( li->workshop_item_id, &item );
							if ( item.item_id != 0 ) {
								if ( Sys_FileExists( va( "workshop/maps/%llu/%s", item.item_id, item.filename ) ) ) {
									mapDownloaded = true;
								}
							}
						}
					}
				} while ( m_messageBoxManager->isShown() && !mapDownloaded );
				
				if ( !m_messageBoxManager->isShown() ) {
					Sys_DPrintf( "[DUKEMP] Workshop download cancelled\n" );
					return;
				}
				m_messageBoxManager->close();
				
			}
		}
		
		m_lobby_id = lobby_id;		
		CSTEAM_JoinLobby( m_lobby_id );
		m_context->GetDocument( "menu-multiplayer-lobby")->SetAttribute( "parent", "menu-main" );
		m_menu->ShowDocument( m_context, "menu-multiplayer-lobby", false );
		UpdateLobbyPage( li );
		
	}
	
}

void GUI::UpdateLobbyList() {
    CSTEAM_UpdateLobbyList();
}

void GUI::StartLobby() {
    Rocket::Core::ElementDocument *lobby_page = m_context->GetDocument("menu-multiplayer-lobby");
    Rocket::Core::Element *start_item = lobby_page->GetElementById("start-lobby");
    start_item->SetProperty("display", "block");
    start_item->SetClass("disabled", true);
    CSTEAM_StartLobby(m_lobby_id);
}

void GUI::OnLobbyCreated(lobby_info_t *lobby_info) {
    m_lobby_id = lobby_info->id;
}

void GUI::OnLobbyListUpdated() {
    Rocket::Core::ElementDocument *menu_page = m_context->GetDocument("menu-multiplayer-join");
    Rocket::Core::Element *menu = menu_page->GetElementById("menu");
    
    lobby_info_t lobby_info;
    char buffer[1000];
    char players[4];
    int j = 0;
    steam_id_t filtered_lobbies[1000];
    if (CSTEAM_NumLobbies()) {
        menu->SetInnerRML("");
        
        for (int i = 0, num_lobbies = CSTEAM_NumLobbies(); i < num_lobbies; i++) {
            steam_id_t lobby_id = CSTEAM_GetLobbyId(i);
            CSTEAM_GetLobbyInfo(lobby_id, &lobby_info);
            if (lobby_info.addon == dnGetAddonId()) {
                if (lobby_info.maxplayers == lobby_info.num_players && lb.show_full == 0)
                    continue;
                if (lobby_info.mode != lb.gamemode && lb.gamemode !=3)
                    continue;
                if ((lb.maps == 0) || (lb.maps == 1 && lobby_info.usermap == 0 && lobby_info.workshop_item_id == 0)
                    || (lb.maps == 2 && lobby_info.usermap == 1 && lobby_info.workshop_item_id == 0)
                    || (lb.maps == 3 && lobby_info.usermap == 0 && lobby_info.workshop_item_id != 0)) {
                    filtered_lobbies[j] = lobby_id;
                    j++;
                }
            }
        }
        
        if (j > 0) {
            for (int i = 0; i < j; i++) {
                Rocket::Core::Element *element = new Rocket::Core::Element("div");   
                steam_id_t lobby_id = filtered_lobbies[i];//CSTEAM_GetLobbyId(i);
                CSTEAM_GetLobbyInfo(lobby_id, &lobby_info);
                sprintf(players, "%d/%d", lobby_info.num_players, lobby_info.maxplayers);
                sprintf(buffer, "<p class=\"column1\">%s</p><p class=\"column2\">%s</p><p class=\"column3\">%s</p><p class=\"column4\">%s</p>", lobby_info.name, lobby_info.mapname, (lobby_info.mode == 0) ? "DM" : "COOP", players);
                element->SetInnerRML(buffer);
                sprintf(buffer, "lobby-%llu", lobby_info.id);
                element->SetId(buffer);
                element->SetAttribute("command", "join-game");
                menu->AppendChild(element);
                m_menu->SetupMenuItem(element);
            }
            m_menu->HighlightItem(menu_page, menu->GetFirstChild()->GetId());
        } else {
            menu->SetInnerRML("<div noanim class='empty' id='stub'>No games found</div>");
            m_menu->HighlightItem(menu_page, "stub");
        }
    } else {
        menu->SetInnerRML("<div noanim class='empty' id='stub'>No games found</div>");
        m_menu->HighlightItem(menu_page, "stub");
    }
//
//    Rocket::Core::String str;
//    menu->GetInnerRML(str);
//    printf("Content: %s\n---------", str.CString());
}

static
void SetElementText(Rocket::Core::ElementDocument *doc, const char *element_id, const char *text) {
    Rocket::Core::Element *e = doc->GetElementById(element_id);
    if (e != NULL) {
        e->SetInnerRML(text);
    } else {
        Sys_DPrintf("Element not found: %s\n", element_id);
    }
}

void GUI::UpdateLobbyPage(lobby_info_t *lobby_info) {
    Rocket::Core::ElementDocument *lobby_page = m_menu->GetCurrentPage(m_context);
    
    if (lobby_page->GetId() != "menu-multiplayer-lobby") {
        return;
    }
	
	if ( lobby_info->version == 0 ) {
		/* lobby_info is not filled, skip */
		return;
	}
    
    Rocket::Core::Element *textarea = lobby_page->GetElementById("chat");
    
    float scroll_top = 0;
    
    if (textarea != NULL) {
        scroll_top = textarea->GetScrollTop();
    }
    
    Rocket::Core::Element *member_div = lobby_page->GetElementById("member-list");
    if (member_div != NULL) {
        member_div->SetInnerRML("");
        for (int i = 0; i < lobby_info->num_players; i++) {
            Rocket::Core::Element *element = new Rocket::Core::Element("div");
            std::string player_name(lobby_info->players[i].name);
            encode(player_name);
            element->SetInnerRML(player_name.c_str());
            member_div->AppendChild(element);
            if (lobby_info->players[i].id != CSTEAM_MyID()) {
                CSTEAM_SetPlayedWith(lobby_info->players[i].id);
            }
        }
    }
    char buffer[100];
    switch (lobby_info->mode) {
        case 0: strcpy(buffer, "DM"); break;
        case 1: strcpy(buffer, "TDM"); break;
        case 2: strcpy(buffer, "COOP"); break;
        default: strcpy(buffer, "Unknown"); break;
    }
    SetElementText(lobby_page, "info-mode", va("Mode: %s", buffer));
    SetElementText(lobby_page, "info-maxplayers", va("Max Players: %d", lobby_info->maxplayers));
    SetElementText(lobby_page, "info-level", va("Map: %s", lobby_info->mapname));
    SetElementText(lobby_page, "info-fraglimit", va("Fraglimit: %d", lobby_info->fraglimit));
    SetElementText(lobby_page, "info-timelimit", va("Timelimit: %d", lobby_info->timelimit));
    SetElementText(lobby_page, "info-markers", va("Respawn Markers: %s", lobby_info->markers ? "ON" : "OFF"));
    SetElementText(lobby_page, "info-friendlyfire", va("Friendly Fire: %s", lobby_info->friendly_fire ? "ON" : "OFF"));
    SetElementText(lobby_page, "info-skill", va("Monster Skill: %d", lobby_info->monster_skill));
    
    Rocket::Core::Element *start_item = lobby_page->GetElementById("start-lobby");
    if (lobby_info->owner != NULL && lobby_info->owner->id == CSTEAM_MyID()) {
        start_item->SetProperty("display", "block");
        start_item->SetClass("disabled", (lobby_info->num_players == 1));
    } else {
        start_item->SetProperty("display", "none");
        start_item->SetClass("disabled", true);
    }
    
    Rocket::Core::Element *invite_item = lobby_page->GetElementById("invite-friend");
    if (lobby_info->owner != NULL && lobby_info->owner->id == CSTEAM_MyID()) {
        invite_item->SetProperty("display", "block");
        invite_item->SetClass("disabled", false);
    } else {
        invite_item->SetProperty("display", "none");
        invite_item->SetClass("disabled", true);
    }
    
    Rocket::Core::Element *client_info_item = lobby_page->GetElementById("client-info");
    if (lobby_info->owner != NULL && lobby_info->owner->id == CSTEAM_MyID()) {
        client_info_item->SetProperty("display", "none");
    } else {
        client_info_item->SetProperty("display", "block");
    }
    
    if (textarea != NULL) {
        textarea->SetScrollTop(scroll_top);
    }
}

bool GUI::GetOptionNumericValue(Rocket::Core::ElementDocument *menu_page, const Rocket::Core::String& option_id, const Rocket::Core::String& prefix, int *pvalue) {
    Rocket::Core::String format;
    format = prefix+"%d";
    Rocket::Core::String eid = m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, option_id))->GetId();
    int value;
    if (sscanf(eid.CString(), format.CString(), &value) == 1) {
        *pvalue = value;
        return true;
    }
    return false;
}

void GUI::OnLobbyEnter(lobby_info_t *lobby_info) {
    lobby_info_t li = { 0 };
	m_invited_lobby = 0;
    CSTEAM_GetLobbyInfo(m_lobby_id, &li);
    UpdateLobbyPage(lobby_info);
}

void GUI::OnLobbyMembersChanged(lobby_info_t *lobby_info) {
}

void GUI::OnLobbyMessage(lobby_message_t *message) {
    lobby_info_t lobby_info;
    CSTEAM_GetLobbyInfo(message->lobby_id, &lobby_info);
    for (int i = 0; i < lobby_info.num_players; i++) {
        if (lobby_info.players[i].id == message->from) {
            Rocket::Core::ElementDocument * doc = m_context->GetDocument("menu-multiplayer-lobby");
            if (doc != NULL) {
                Rocket::Core::Element * textarea =  doc->GetElementById("chat");
                if (textarea != NULL) {
                    Rocket::Core::Variant * value =  textarea->GetAttribute("value");
                    Rocket::Core::String valueString;
                    if (value != NULL) {
                        valueString = value->Get<Rocket::Core::String>();
                    }
                    textarea->SetAttribute("value", va("%s%s: %s\n", valueString.CString(), lobby_info.players[i].name, message->text));
                    textarea->SetScrollTop(textarea->GetScrollHeight());
                }
            }
        }
    }

    
}

static
void NewNetworkGame(lobby_info_t *lobby_info);

void GUI::OnLobbyStarted(lobby_info_t *lobby_info) {
	m_lobbyInfoRequestManager->requestLobbyInfo( m_lobby_id );
	
	double cycleStarted = Sys_GetTicks();
	bool messageShown = false;
	
	do {
		if ( !messageShown && Sys_GetTicks() - cycleStarted > 1.000 ) {
			m_messageBoxManager->showMessageBox( "Updating lobby info...", NULL );
		}
		doGameLoop();
	} while ( !m_lobbyInfoRequestManager->isComplete() && !m_lobbyInfoRequestManager->isTimedOut() );
	
	if ( m_lobbyInfoRequestManager->isComplete() ) {
		Enable( false );
		ps[myconnectindex].gm &= ~MODE_MENU;
		m_menu->ShowDocument( m_context, "menu-ingame", false );

		NewNetworkGame( m_lobbyInfoRequestManager->getLobbyInfo() );

		CSTEAM_LeaveLobby( m_lobby_id );
		m_lobby_id = 0;
	} else {
		m_messageBoxManager->showMessageBox( "Could not start the game", "Close", NULL );
		m_messageBoxManager->wait();
	}
}

void GUI::OnWorkshopSubscribe(workshop_item_t *item, int status) {
	m_workshopRequestManager->onWorkshopSubscribe( item, status );
}

void GUI::OnBigPictureTextUpdate(const char* data, int status) {
    if (dnGamepadConnected()) {
        CSTEAM_SendLobbyMessage(m_lobby_id, data);
    }
}

void GUI::OnLobbyDataUpdate(lobby_info_t *lobby_info, int status) {
    if (lobby_info->version == 0) { /* empty lobby_info, the code below tends to dereference NULL pointers */
        return;
    }
	
	m_lobbyInfoRequestManager->onLobbyDataUpdate( lobby_info, status );
    
	if (status != 0) {
		return;
	}
	
	UpdateLobbyPage(lobby_info);
	if (lobby_info->num_players > 1) {
		sound(EXITMENUSOUND);
	}
}

void GUI::OnLobbyJoinInvite(steam_id_t lobby_id) {
    if ( dnGameModeSet() ) {
        gameexit(" ");
    }
    m_menu->ShowDocument( m_context, "menu-main", false );
	m_invited_lobby = lobby_id;
	UpdateLobbyList();
    JoinLobby( m_invited_lobby );
}

Uint32 GUI_NewTimerCallback(Uint32 interval, void* param) {
    SDL_Event event = { 0 };
    event.type = SDL_USEREVENT;
    event.user.type = SDL_USEREVENT;
    event.user.code = 1;
    SDL_PushEvent(&event);
    return 0;
}

void GUI::Poll() {
}

extern "C" long quittimer;

void GUI::Render() {
    double ticks = Sys_GetTicks();
    GLboolean fogOn = glIsEnabled(GL_FOG);
    if (ticks - m_last_poll > 100) { /* don't poll faster than 10 times per second */
        Poll();
    }
    
    if (quittimer > totalclock && gamequit != 0) {
        //dnQuitToTitle("Timed out");
//        Sys_DPrintf("Timed out");
    }
    
    if (fogOn == GL_TRUE) {
        glDisable(GL_FOG);
    }
    
    CSTEAM_RunFrame();
	Enable(dnMenuModeSet() && m_enabled_for_current_frame);
	m_enabled_for_current_frame = false;
	if (m_enabled) {
        GL_SetUIMode(m_width, m_height, m_menu_scale, m_menu_offset_x, m_menu_offset_y);

		if (m_menu_offset_y != 0 && !dnGameModeSet()) {
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		m_animation->UpdateAnimation();
		m_context->Update();
		m_context->Render();
        if (m_draw_strips && m_menu_offset_y > 0) {
			DrawStrips();
        }
        if ( m_setBrightness != -1 && m_setBrightness != ud.brightness && Sys_GetTicks() - m_lastBrightnessUpdate > 500 ) {
            dnSetBrightness( m_setBrightness );
            m_lastBrightnessUpdate = Sys_GetTicks();
        }
	}
    if (fogOn == GL_TRUE) {
        glEnable(GL_FOG);
    }
}

void GUI::Enable(bool v) {
	if (v != m_enabled) {
		KB_FlushKeyboardQueue();
		KB_ClearKeysDown();
		m_enabled = v;
		if (m_enabled) {
            dnSetMouseInputMode( DN_MOUSE_SDL );
            dnGrabMouse( 0 );
            if (dnGameModeSet()) {
                m_menu->ShowDocument(m_context, menu_to_open, false);
                if (menu_to_open == "menu-ingame" ) {
                    Rocket::Core::ElementDocument *doc = m_context->GetDocument(menu_to_open);
                    Rocket::Core::Element *item = doc->GetElementById("save-game");
                    item->SetClass("disabled", (ud.multimode > 1));
                    item->SetProperty("display", (ud.multimode > 1) ? "none" : "block");
                    item = doc->GetElementById("load-game");
                    item->SetClass("disabled", (ud.multimode > 1));
                    item->SetProperty("display", (ud.multimode > 1) ? "none" : "block");
                    item = doc->GetElementById("quit");
                    item->SetClass("disabled", (ud.multimode > 1));
                    item->SetProperty("display", (ud.multimode > 1) ? "none" : "block");
                    item = doc->GetElementById("stop");
                    if (ud.multimode > 1 && connecthead == myconnectindex) {
                        item->SetInnerRML("STOP SERVER");
                    } else if (ud.multimode > 1 && connecthead != myconnectindex) {
                        item->SetInnerRML("DISCONNECT");
                    } else {
                        item->SetInnerRML("QUIT TO TITLE");
                    }
                    
                }
            } else if (m_join_on_launch) {
                m_show_press_enter = false;
                m_join_on_launch = false;
                m_menu->ShowDocument(m_context, "menu-main", false);
                JoinLobby( m_invited_lobby );
            } else {
                m_menu->ShowDocument(m_context, m_show_press_enter ? "menu-start" : "menu-main", false);
                
                char message[1024];
                dnGetQuitMessage(message);
                Sys_DPrintf("Quit message: %s\n", message);
                if (message[0] != 0) {
                    ShowErrorMessage("menu-main", message);
                }

            }
			if (dnGameModeSet()) {
				m_context->GetDocument("menu-bg")->Hide();
			} else {
				Rocket::Core::ElementDocument *menubg = m_context->GetDocument("menu-bg");
				menubg->Show();
			}
                        
            ResetMouse();
		} else {
            dnSetMouseInputMode( DN_MOUSE_RAW );
            dnGrabMouse( 1 );
			m_context->ShowMouseCursor(false);
		}
	}
}

void GUI::ResetMouse() {
    VideoMode vm;
    dnGetCurrentVideoMode(&vm);
    m_mouse_x = vm.width/2;
    m_mouse_y = vm.height/2;
}

template <typename T>
bool in_range(T v, T min, T max, int *offset) {
	*offset = v - min;
	return (v >= min) && (v <= max);
}

static bool
TranslateRange(dnKey min, dnKey max, dnKey key, Rocket::Core::Input::KeyIdentifier base, Rocket::Core::Input::KeyIdentifier *result) {
	int offset;
	if (in_range(min, max, key, &offset)) {
		*result = (Rocket::Core::Input::KeyIdentifier)(base + offset);
		return true;
	}
	return false;
}

static Rocket::Core::Input::KeyIdentifier
translateKey(dnKey key) {
	using namespace Rocket::Core::Input;
	KeyIdentifier result;
	if (TranslateRange(SDL_SCANCODE_0, SDL_SCANCODE_9, key, KI_0, &result)) { return result; }
	if (TranslateRange(SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_9, key, KI_0, &result)) { return result; }
	if (TranslateRange(SDL_SCANCODE_F1, SDL_SCANCODE_F12, key, KI_F1, &result)) { return result; }
	if (TranslateRange(SDL_SCANCODE_A, SDL_SCANCODE_Z, key, KI_A, &result)) { return result; }
	if (key == SDL_SCANCODE_CAPSLOCK) { return KI_CAPITAL; }
	if (key == SDL_SCANCODE_TAB) { return KI_TAB; }
    if (key == SDL_SCANCODE_HOME) { return KI_HOME; }
    if (key == SDL_SCANCODE_END) { return KI_END; }
    if (key == SDL_SCANCODE_LEFT) { return KI_LEFT; }
    if (key == SDL_SCANCODE_RIGHT) { return KI_RIGHT; }
    if (key == SDL_SCANCODE_BACKSPACE) { return KI_BACK; }
	return KI_UNKNOWN;
}

#define KEYDOWN(ev, k) (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == k)

static int MouseButtonID( int button ) {
    if ( button < 4 ) {
        return button - 1;
	}
    else {
        return button + 1;
	}
}

void GUI::ProcessKeyDown( dnKey key ) {
	
}

extern "C" void dnReleaseKey(dnKey key);

bool GUI::InjectEvent(SDL_Event *ev) {
	bool retval = false;
        
#if !CLASSIC_MENU
	if ( dnKeyJustPressed( SDL_SCANCODE_ESCAPE ) || ((dnKeyJustPressed((dnKey)DNK_GAMEPAD_B)) && !m_waiting_for_key) || (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_RIGHT && !m_waiting_for_key)) {
        if (m_waiting_for_key) {
            m_context->GetDocument("key-prompt")->Hide();
            m_waiting_for_key = false;
		} else 	{
			m_draw_strips = false;
            
			if ( dnMenuModeSet() ) {
				if (!m_menu->GoBack(m_context)) {
					menu_to_open = "menu-ingame";
					dnHideMenu();
				}
				return true;
			}
		}
	}
#endif
#ifdef _DEBUG
	if (KEYDOWN(ev, SDLK_F8)) {
		Reload();
	}
#endif
	
	if (IsEnabled()) {
		retval = true;

        if ( m_waiting_for_key ) {
			for ( int i = 0; i < DN_MAX_KEYS; i++ ) {
				dnKey key = (dnKey)i;
				if ( dnKeyJustPressed( key ) ) {
					m_pressed_key = key;
                    if (ev->type == SDL_MOUSEWHEEL) {
                        HideKeyPrompt();
						const char *id = dnGetKeyName(m_pressed_key);
                        AssignFunctionKey(m_waiting_menu_item->GetId(), id, m_waiting_slot);
                        InitKeysSetupPage(m_context->GetDocument("menu-keys-setup"));
                    }
                    break;
				} else if ( dnKeyJustReleased( key ) ) {
					if ( m_pressed_key == key ) {
						HideKeyPrompt();
						const char *id = dnGetKeyName(m_pressed_key);
                        AssignFunctionKey(m_waiting_menu_item->GetId(), id, m_waiting_slot);
                        InitKeysSetupPage(m_context->GetDocument("menu-keys-setup"));
					}
					break;
				}
			}
        } else {
			
			/* process keys */
			for ( int i = 0; i < DN_MAX_KEYS; i++ ) {
				dnKey key = (dnKey)i;
				
				if ( dnKeyJustPressed( key ) ) {
					/* process mouse buttons */
					if ( key >= DNK_MOUSE_FIRST && key < DNK_MOUSE4 ) {
						int mouseButton = key - DNK_MOUSE_FIRST;
						if ( mouseButton == 1 || mouseButton == 2 ) {
							mouseButton = 2 - mouseButton;
						}
						m_context->ProcessMouseButtonDown( mouseButton, 0 );
					}
					/* process rest of the keys */
					Rocket::Core::Element *focus_target = m_menu->GetFocusTarget( m_menu->GetHighlightedItem( m_menu->GetCurrentPage( m_context ) ) );
                    if ( focus_target != NULL ) {
                        if ( key == SDL_SCANCODE_LEFT ) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_LEFT, 0 );
                        } else if ( key == SDL_SCANCODE_RIGHT) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_RIGHT,  0);
                        } else if ( key == SDL_SCANCODE_HOME) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_HOME, 0 );
                        } else if ( key == SDL_SCANCODE_END) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_END, 0 );
                        } else if ( key == SDL_SCANCODE_RETURN || key == DNK_GAMEPAD_A) {
                            dnReleaseKey((dnKey)DNK_GAMEPAD_A);
                            m_menu->DoItemAction( ItemActionEnter, m_context);
                        } else if ( key == SDL_SCANCODE_DELETE ) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_DELETE, 0 );
                        } else if ( key == SDL_SCANCODE_BACKSPACE ) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_BACK, 0 );
                        } else if ( key == DNK_MENU_DOWN ) {
                            m_menu->HighlightNextItem( m_context );
                        } else if ( key == DNK_MENU_UP ) {
                            m_menu->HighlightPreviousItem( m_context );
                        } else {
                            m_menu->UpdateFocus( m_context );
                        }
                    } else {
						if ( key == SDL_SCANCODE_LEFT ) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_LEFT, 0 );
						} else if  ( key == DNK_MENU_PREV_OPTION ) {
                            m_menu->SetPreviousItemValue( m_context );
                        } else if ( key == SDL_SCANCODE_RIGHT ) {
                            m_context->ProcessKeyDown(Rocket::Core::Input::KI_RIGHT, 0);
						} else if ( key == DNK_MENU_NEXT_OPTION ) {
                            m_menu->SetNextItemValue( m_context );
                        } else if ( key == SDL_SCANCODE_HOME ) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_HOME, 0 );
                        } else if ( key == SDL_SCANCODE_END ) {
                            m_context->ProcessKeyDown( Rocket::Core::Input::KI_END, 0 );
                        } else if ( key == DNK_MENU_DOWN ) {
                            m_menu->HighlightNextItem( m_context );
                        } else if ( key == DNK_MENU_UP ) {
                            m_menu->HighlightPreviousItem( m_context );
                        } else if ( key == DNK_MENU_ENTER ) {
                            m_menu->DoItemAction( ItemActionEnter, m_context );
                        } else if ( key == DNK_MENU_CLEAR ) {
                            m_menu->DoItemAction( ItemActionClear, m_context );
                        } else if ( key == DNK_MENU_REFRESH ) {
                            m_menu->DoItemAction( ItemActionRefresh, m_context );
                        } else if ( key == DNK_MENU_RESET ) {
                            m_menu->DoItemAction( ItemActionReset, m_context );
                        } else {
                            m_menu->UpdateFocus( m_context );
                        }
                    }
				} else if ( dnKeyJustReleased( key ) ) {
					/* process mouse buttons */
					if ( key >= DNK_MOUSE_FIRST && key < DNK_MOUSE4 ) {
						int mouseButton = key - DNK_MOUSE_FIRST;
						if ( mouseButton == 1 || mouseButton == 2 ) {
							mouseButton = 2 - mouseButton;
						}
						m_context->ProcessMouseButtonUp( mouseButton, 0 );
					} else {
						m_context->ProcessKeyUp( translateKey( key ), 0 );
					}
				}
			}
			
			/* process other events */
            switch (ev->type) {
                case SDL_USEREVENT: {
                    if (ev->user.code == 1) {
                        //PingFailed();
                    } else if (ev->user.code == 2) {
                        CSTEAM_InviteFriends(m_lobby_id);
                    }
                    break;
                }

                case SDL_MOUSEMOTION: {
                    m_context->ShowMouseCursor( true );
                    SDL_GetMouseState( &m_mouse_x, &m_mouse_y );
                    Rocket::Core::Vector2i mpos = TranslateMouse( m_mouse_x, m_mouse_y );
                    m_context->ProcessMouseMove( mpos.x, mpos.y, 0 );
                    break;
                }
					
                case SDL_MOUSEWHEEL:
                    if (ev->wheel.y > 0)
                        m_context->ProcessMouseWheel(-2, 0);
                    if (ev->wheel.y < 0)
                        m_context->ProcessMouseWheel(2, 0);
                    break;
					 
                case SDL_TEXTINPUT:
                {
                    Rocket::Core::Element *focus_target = m_menu->GetFocusTarget(m_menu->GetHighlightedItem(m_menu->GetCurrentPage(m_context)));
                    if (focus_target != NULL) {
                        int j = 0;
                        m_menu->UpdateFocus(m_context);
#if UNICODE_INPUT
						m_context->ProcessTextInput( ev->text.text );
#else
						/* ASCII filter, we don't have unicode fonts */
						for ( const char *p = ev->text.text; *p != 0; p++ ) {
							if ( *p < '\x80' ) {
								m_context->ProcessTextInput( *p );
							}
						}
#endif
                    }
                    break;
                }
                default:
                    retval = false;
                    break;
            }
        }
	}
	return retval;
}

void GUI::HideKeyPrompt() {
    m_waiting_for_key = false;
    m_context->GetDocument("key-prompt")->Hide();
}

void GUI::TimePulse() {
}

Rocket::Core::Vector2i GUI::TranslateMouse(int x, int y) {
	int nx = (int)(x/m_menu_scale - m_menu_offset_x);
	int ny = (int)(y/m_menu_scale - m_menu_offset_y);
	return Rocket::Core::Vector2i(nx, ny);
}

void GUI::EnableForCurrentFrame() {
	m_enabled_for_current_frame = true;
}

static
void NewGame(int skill, int episode, int level) {
	GameDesc gamedesc = { 0 };

	gamedesc.level				= level;
	gamedesc.volume				= episode;

	gamedesc.player_skill		= skill;
	gamedesc.monsters_off		= 0;
	//respawn_monsters is based on player_skill being 4
	gamedesc.respawn_monsters	= (gamedesc.player_skill == 4) ? 1 : 0;
	gamedesc.respawn_inventory	= 0;
	gamedesc.coop				= 0;
	gamedesc.respawn_items		= 0;

	gamedesc.marker				= 0;
	gamedesc.ffire				= 1;

	gamedesc.SplitScreen		= 0;		// JEP COOP
	gamedesc.ffire				= 0;
	dnNewGame(&gamedesc);
}

void GUI::NewNetworkGame(lobby_info_t *lobby_info) {
	GameDesc gamedesc = { 0 };
    
    Sys_DPrintf("*** STARTING NETWORK GAME ***\n");
    steam_id_t my_id = CSTEAM_MyID();
    steam_id_t owner_id = lobby_info->owner->id;
    
    gamedesc.netgame = 1;
    gamedesc.own_id = my_id;
    gamedesc.numplayers = lobby_info->num_players;
    
    for (int i = 0; i < lobby_info->num_players; i++) {
        steam_id_t player_id = lobby_info->players[i].id;
        Sys_DPrintf("[%s]Player #%d: %s\n", player_id == my_id ? "*" : " ", i, CSTEAM_FormatId(player_id));
        gamedesc.other_ids[i] = lobby_info->players[i].id;
    }
    
//    /* put owner id at first array position */
//    for (int i = 0; i < lobby_info->num_players; i++) {
//        if (gamedesc.other_ids[i] == owner_id) {
//            gamedesc.other_ids[i] = gamedesc.other_ids[0];
//            gamedesc.other_ids[0] = owner_id;
//            break;
//        }
//    }
    
    gamedesc.level				= 0;
    gamedesc.volume				= 0;

    if (!lobby_info->usermap && lobby_info->workshop_item_id == 0) {
        int e, m;
        if (sscanf(lobby_info->mapname, "e%dm%d", &e, &m) == 2 || sscanf(lobby_info->mapname, "E%dM%d", &e, &m) == 2) {
            gamedesc.volume = e-1;
            gamedesc.level = m-1;
        }
    } else {
        gamedesc.volume = 0;
        gamedesc.level = 7;
        if(lobby_info->workshop_item_id > 0) {
            workshop_item_t item;
            dnUnsetWorkshopMap();
            CSTEAM_GetWorkshopItemByID(lobby_info->workshop_item_id,  &item);
            dnSetWorkshopMap(lobby_info->mapname, va("/workshop/maps/%llu/%s", item.item_id, item.filename));
            if (workshopmap_group_handler == -1) {
                ShowErrorMessage("menu-multiplayer-lobby", "The map isn't downloaded yet");
                return;
            }

        } else
            dnSetUserMap(lobby_info->mapname);
    }
    
    gamedesc.coop				= lobby_info->mode == 2;
    
	gamedesc.player_skill		= lobby_info->monster_skill ? lobby_info->monster_skill : 1;
	gamedesc.monsters_off		= lobby_info->monster_skill == 0;
	//respawn_monsters is based on player_skill being 4
	gamedesc.respawn_monsters	= (gamedesc.player_skill == 4) ? 1 : 0;
	gamedesc.respawn_inventory	= 1;

	gamedesc.respawn_items		= 1;
    
	gamedesc.marker				= lobby_info->markers ? 1 : 0;
	gamedesc.ffire				= lobby_info->friendly_fire ? 1 : 0;
    
    gamedesc.fraglimit = lobby_info->fraglimit;
    gamedesc.timelimit = lobby_info->timelimit;
    
	dnEnterMultiMode( lobby_info );
	
	dnNewGame(&gamedesc);
}

static
void SaveVideoMode(VideoMode *vm) {
	/* update config */
	ScreenWidth = vm->width;
	ScreenHeight = vm->height;
	ScreenBPP = vm->bpp;
	ScreenMode = vm->fullscreen;
    vscrn();
}

void GUI::ReadChosenSkillAndEpisode(int *pskill, int *pepisode) {
	Rocket::Core::Element *skill = m_menu->GetHighlightedItem(m_context, "menu-skill");
    assert(skill != NULL);
    sscanf(skill->GetId().CString(), "skill-%d", pskill);
    if (dnGetAddonId() != 0) {
        *pepisode = dnGetAddonEpisode();
        return;
    } else if (dnIsUserMap()) { // usermap
        *pepisode = 0;
        return;
    } else {
        Rocket::Core::Element *episode = m_menu->GetHighlightedItem(m_context, "menu-episodes");
        assert(episode != NULL);
        sscanf(episode->GetId().CString(), "episode-%d", pepisode);
        return;
    }
    assert(false);
}

Uint32 InviteFriends_TimerCallback(Uint32 interval, void* param) {
    SDL_Event event = { 0 };
    event.type = SDL_USEREVENT;
    event.user.type = SDL_USEREVENT;
    event.user.code = 2;
    SDL_PushEvent(&event);
    return 0;
}

bool GUI::DoCommand(Rocket::Core::Element *element, const Rocket::Core::String& command) {
//	Sys_DPrintf("[GUI ] Command: %s\n", command.CString());
    bool result = true;
	
	if ( m_messageBoxManager->doCommand( element, command ) ) {
		return true;
	}
	
	if (command == "game-start") {
		int skill, episode;
		ReadChosenSkillAndEpisode(&skill, &episode);
		Enable(false);
        ps[myconnectindex].gm &= ~MODE_MENU;
		m_menu->ShowDocument(m_context, "menu-ingame", false);
        // for usermap level should be 7 and episode should be 0
		NewGame(skill, episode, (dnIsUserMap() ? 7 : 0));
	} else if (command == "game-quit") {
        QuitGameCommand(element);
    } else if (command == "game-quit-immediately") {
        dnQuitGame();
    } else if (command == "apply-video-mode") {
		VideoMode vm;
		ReadChosenMode(&vm);
		dnChangeVideoMode(&vm);
		UpdateApplyStatus();
	} else if (command == "load-game") {
            LoadGameCommand(element);
    } else if (command == "save-game") {
            SaveGameCommand(element);
    } else if (command == "game-stop") {
        QuitToTitleCommand(element);
    } else if (command == "confirm-yes") {
        if (m_action_to_confirm != NULL) {
            if (m_action_to_confirm->back_page != NULL) {
                m_action_to_confirm->back_page->Hide();
            }
            m_menu->GoBack(m_context);
            m_action_to_confirm->Yes();
            SetActionToConfirm(NULL);
        }
    } else if (command == "confirm-no") {
        if (m_action_to_confirm != NULL) {
            m_action_to_confirm->No();
            
            if (m_action_to_confirm->back_page != NULL) {
                m_action_to_confirm->back_page->Hide();
            }
            SetActionToConfirm(NULL);
            
            m_menu->GoBack(m_context);
        }
    } else if (command == "resume-game") {
        menu_to_open = "menu-ingame";
        dnHideMenu();
    } else if (command == "restore-video-mode") {
        dnChangeVideoMode(&m_backup_video_mode);
        SaveVideoMode(&m_backup_video_mode);
        m_menu->GoBack(m_context, false);
    } else if (command == "restore-video-mode-esc") {
        dnChangeVideoMode(&m_backup_video_mode);
        SaveVideoMode(&m_backup_video_mode);
    } else if (command == "save-video-mode") {
        SaveVideoMode(&m_new_video_mode);
        m_menu->GoBack(m_context, false);
    } else if (command == "open-hub") {
        CSTEAM_OpenCummunityHub();
    } else if (command == "start") {
        m_show_press_enter = false;
		//printf(NULL);
    } else if (command == "load-workshopitem") {
        Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-usermaps");
        Rocket::Core::Element *map = m_menu->GetHighlightedItem(doc);
        workshop_item_t item;
        steam_id_t item_id;
        sscanf(map->GetProperty("item-id")->ToString().CString(), "%llu", &item_id);
        CSTEAM_GetWorkshopItemByID(item_id, &item);
        dnSetWorkshopMap(item.itemname, va("/workshop/maps/%llu/%s", item.item_id, item.filename));
        if (workshopmap_group_handler == -1) {
            ShowErrorMessage(doc->GetId().CString(), "The map isn't downloaded yet");
        } else {
            m_menu->ShowDocument(m_context, "menu-skill");
        }
    } else if (command == "load-usermap") {
        Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-usermaps");
        Rocket::Core::Element *map = m_menu->GetHighlightedItem(doc);
        dnSetUserMap(map->GetProperty("map-name")->ToString().CString());
        m_menu->ShowDocument(m_context, "menu-skill");
    } else if (command == "create-lobby") {
        CreateLobby();
    } else if (command == "join-game") {
        steam_id_t lobby_id = 0;
        if (sscanf(element->GetId().CString(), "lobby-%llu", &lobby_id) == 1) {
            JoinLobby(lobby_id);
        }
    } else if (command == "start-lobby") {
        StartLobby();
    } else if (command == "leave-lobby") {
        LeaveLobbyCommand(element);
        result = false;
    } else if (command == "reload-lobby-info") {
        lobby_info_t li;
        CSTEAM_GetLobbyInfo(m_lobby_id, &li);
    } else if (command == "choose-mp-level") {
        Rocket::Core::String mapname = element->GetAttribute("mapname")->Get<Rocket::Core::String>();
        bool usermap = element->IsClassSet("usermap");
        Rocket::Core::Element *option = m_context->GetDocument("menu-multiplayer-create")->GetElementById("mp-level");
        if (element->IsClassSet("workshop")) {
            option->SetAttribute("item-id", element->GetAttribute("item-id")->Get<Rocket::Core::String>());
        }
        option->SetClass("workshop", element->IsClassSet("workshop"));
        option->SetClass("usermap", usermap);
        option->SetAttribute("mapname", mapname);
        option->SetInnerRML(mapname);
        m_menu->ShowDocument(m_context, "menu-multiplayer-create", false);
    } else if (command == "invite-friend") {
        SDL_AddTimer(200, InviteFriends_TimerCallback, NULL);
    } else if (command == "send-chat-message") {
        Rocket::Core::Element *focus_target = m_menu->GetFocusTarget(element);
        if (focus_target != NULL) {
//            if (dnGamepadConnected() && CSTEAM_ShowGamepadTextInput("", 140)) {
//            } else {
                Rocket::Core::Variant *value = focus_target->GetAttribute("value");
                if (value != NULL) {
                    Rocket::Core::String message = focus_target->GetAttribute("value")->Get<Rocket::Core::String>();
                    focus_target->SetAttribute("value", "");
                    CSTEAM_SendLobbyMessage(m_lobby_id, message.CString());
                }
//            }
        }
    }
    return result;
}
	
void QuitToTitle() {
//	if ( gamequit == 0 &&
//		dnIsInMultiMode() &&
//		(ps[myconnectindex].gm & MODE_GAME) &&
//		dnIsHost()
//		)
//	{
//        gamequit = 1;
//        quittimer = totalclock + 120;
//	} else {
//		gameexit( " " );
//	}
	if ( dnIsInMultiMode() && dnIsHost() ) {
        gamequit = 1;
        quittimer = totalclock + 120;
	} else {
		gameexit( " " );
	}
}

void GUI::QuitToTitleCommand(Rocket::Core::Element *element) {

    struct QuitToTitleAction: public ConfirmableAction {
        Rocket::Core::Context *context;
        RocketMenuPlugin *menu;
        QuitToTitleAction(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu):ConfirmableAction(back_page),context(context), menu(menu) {}
        virtual void Yes() {
            QuitToTitle();
            menu->ShowDocument(context, "menu-main", false);
        }
    };
    Rocket::Core::ElementDocument *page = element->GetOwnerDocument();
    ShowConfirmation(new QuitToTitleAction(page, m_context, m_menu), "yesno", "Quit To Title?");
} 

void GUI::LeaveLobbyCommand(Rocket::Core::Element *element) {
#if 0
    struct LeaveLobbyAction: public ConfirmableAction {
        Rocket::Core::Context *context;
        RocketMenuPlugin *menu;
        steam_id_t m_lobby_id;
        LeaveLobbyAction(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu, steam_id_t lobby_id):ConfirmableAction(back_page),context(context), menu(menu), m_lobby_id(lobby_id) {}
        virtual void Yes() {
            if (m_lobby_id != 0) {
                CSTEAM_LeaveLobby(m_lobby_id);
                m_lobby_id = 0;
            }
            menu->ShowDocument(context, "menu-multiplayer", false);
        }
    };
    Rocket::Core::ElementDocument *page = element->GetOwnerDocument();
    ShowConfirmation(new LeaveLobbyAction(page, m_context, m_menu, m_lobby_id), "yesno", "Leave this lobby?");
#else
	m_messageBoxManager->showMessageBox( "Leave this lobby?", "Yes", "No", NULL );
	do {
		doGameLoop();
	} while ( m_messageBoxManager->isShown() && m_lobby_id != 0 );
	
	if ( m_messageBoxManager->wait() == 0 ) {
		CSTEAM_LeaveLobby( m_lobby_id );
		m_lobby_id = 0;
		m_menu->GoBack( m_context );
	}
	
#endif
}

void GUI::QuitGameCommand(Rocket::Core::Element *element) {

    struct QuitGameAction: public ConfirmableAction {
        QuitGameAction(Rocket::Core::ElementDocument *page):ConfirmableAction(page){}
        virtual void Yes() {
            dnQuitGame();
        }
        virtual void OnClose() {
        }
    };
    
    Rocket::Core::ElementDocument *page = element->GetOwnerDocument();
    ShowConfirmation(new QuitGameAction(page), "yesno", "Quit Game?");
}

void GUI::SaveGameCommand(Rocket::Core::Element *element) {
    int slot;
    struct savehead saveh;
    
    struct SaveGameAction: public ConfirmableAction {
        int slot;
        SaveGameAction(Rocket::Core::ElementDocument *back_page, int slot):ConfirmableAction(back_page),slot(slot){}
        virtual void Yes() {
            dnSaveGame(slot);
        }
    };
        
    if (sscanf(element->GetId().CString(), "slot%d", &slot) == 1) {
        if (loadpheader(slot, &saveh)) {
            dnSaveGame(slot);
        } else {
            ShowConfirmation(new SaveGameAction(element->GetOwnerDocument(), slot), "yesno", "Save Game?");
        }
    }
}

void GUI::LoadGameCommand(Rocket::Core::Element *element) {
    struct LoadGameAction: public ConfirmableAction {
            Rocket::Core::Context *context;
            RocketMenuPlugin *menu;
            int slot;
            LoadGameAction(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu, int slot):ConfirmableAction(back_page),context(context),menu(menu),slot(slot){}
            virtual void Yes() {
                if (dnLoadGame(slot) == 0) {
                    menu->ShowDocument(context, "menu-ingame", false);
                }
            }
        };

    int slot;
    if (sscanf(element->GetId().CString(), "slot%d", &slot) == 1 && !element->IsClassSet("empty")) {
            if (ps[myconnectindex].gm & MODE_GAME) {
                LoadGameAction *action = new LoadGameAction(element->GetOwnerDocument(), m_context, m_menu, slot);
                ShowConfirmation(action, "yesno", "Load Game?");
            } else {
                if (dnLoadGame(slot) == 0) {
                    m_menu->ShowDocument(m_context, "menu-ingame", false);
                }
            }
        }
}

void GUI::ShowConfirmation(ConfirmableAction *action, const Rocket::Core::String& document, const Rocket::Core::String& text, const Rocket::Core::String& default_option) {
    Rocket::Core::ElementDocument *page = action->back_page;
    Rocket::Core::ElementDocument *doc = m_context->GetDocument(document);
    Rocket::Core::Element *text_element = doc->GetElementById("question");

    text_element->SetInnerRML(text);

    m_menu->ShowDocument(m_context, document);
    m_menu->HighlightItem(doc, default_option);
    doc->PullToFront();
    page->Show();
    page->PushToBack();
    action->doc = doc;
    SetActionToConfirm(action);
}

void GUI::PopulateOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element) {
	char buffer[64];
    if (menu_item->GetId() == "video-mode") {
        VideoModeList video_modes;
        dnGetVideoModeList(video_modes);
        for (VideoModeList::iterator i = video_modes.begin(); i != video_modes.end(); i++) {
            Rocket::Core::Element *option = Rocket::Core::Factory::InstanceElement(options_element, "option", "option", Rocket::Core::XMLAttributes());
            sprintf(buffer, "%dx%d", i->width, i->height);
            option->SetInnerRML(buffer);
            sprintf(buffer, "%dx%d@%d", i->width, i->height, i->bpp);
            option->SetId(buffer);
            options_element->AppendChild(option);
            option->RemoveReference();
        }
    } else if (menu_item->GetId() == "max-fps") {
        Rocket::Core::Element *option = Rocket::Core::Factory::InstanceElement(options_element, "option", "option", Rocket::Core::XMLAttributes());
        option->SetInnerRML("OFF");
        option->SetId("fps-0");
        options_element->AppendChild(option);
        option->RemoveReference();
        for (int i = 30; i < 190; i+=10) {
            Rocket::Core::Element *option = Rocket::Core::Factory::InstanceElement(options_element, "option", "option", Rocket::Core::XMLAttributes());
            sprintf(buffer, "%d", i);
            option->SetInnerRML(buffer);
            sprintf(buffer, "fps-%d", i);
            option->SetId(buffer);
            options_element->AppendChild(option);
            option->RemoveReference();
        }
    }
}

void GUI::DidOpenMenuPage(Rocket::Core::ElementDocument *menu_page) {
	Rocket::Core::String page_id(menu_page->GetId());
    
    intomenusounds();
    
    m_menu->UpdateFocus(m_context);
    
	if (page_id != "menu-main" && page_id != "video-confirm" && page_id != "yesno" && !menu_page->HasAttribute("default-item")) {
		m_menu->HighlightItem(m_menu->GetMenuItem(menu_page, 0));
	}
	if (page_id == "menu-video") {
        InitVideoOptionsPage(menu_page);
    } else if (page_id == "menu-sound") {
        InitSoundOptionsPage(menu_page);
    } else if (page_id == "menu-game-options") {
        InitGameOptionsPage(menu_page);
    } else if (page_id == "menu-options") {
        if (m_need_apply_video_mode) {
            ApplyVideoMode(menu_page);
            m_need_apply_video_mode = false;
			m_need_apply_vsync = false;
        } else if (m_need_apply_vsync) {
            Rocket::Core::Element *option_element = m_menu->GetActiveOption(m_menu->GetMenuItem(m_context->GetDocument("menu-video"), "vertical-sync"));
            Rocket::Core::String str;
            
            if (option_element != NULL) {
                str = option_element->GetId();
                
                ud.vsync = str == "vsync-on" ? 1 : 0;
                
                VideoMode vm;
                dnGetCurrentVideoMode(&vm);
                dnChangeVideoMode(&vm);
                m_need_apply_vsync = false;
                vscrn();
            }
        }
        Rocket::Core::Element *gamepad = menu_page->GetElementById("gamepad-setup");
        if (dnGamepadConnected()) {
            gamepad->SetClass("disabled", false);
        } else {
            gamepad->SetClass("disabled", true);
        }
    } else if (page_id == "menu-keys-setup") {
        InitKeysSetupPage(menu_page);
    } else if (page_id == "menu-mouse-setup") {
        InitMouseSetupPage(menu_page);
    } else if (page_id == "menu-gamepad-setup") {
        InitGamepadSetupPage(menu_page);
    } else if (page_id == "menu-load" || page_id == "menu-save") {
        InitLoadPage(menu_page);
    } else if (page_id == "menu-usermaps") {
        CSTEAM_UpdateWorkshopItems();
        InitUserMapsPage(page_id.CString());
    } else if (page_id == "menu-episodes") {
        dnSetUserMap(NULL);
    } else if (page_id == "menu-multiplayer-join") {
        InitLobbyList(menu_page);
    } else if (page_id == "menu-multiplayer-maps") {
        InitMpMapsPage(menu_page);
    } else if (page_id == "menu-lobby-filter"){
        InitLobbyFilterPage(menu_page);
    } else if (page_id == "menu-multiplayer") {
        menu_page->SetAttribute("parent", "menu-main");
        if (!CSTEAM_Online()) {
            m_menu->GoBack(m_context);
            ShowErrorMessage("menu-main", "Please login in Steam");
        }
    } else if (page_id == "menu-main") {
        Rocket::Core::Element *mp =  menu_page->GetElementById("multiplayer");
        if (mp == NULL) return;
        if (show_mutiplayer_info) {
            mp->SetAttribute("submenu", "menu-mpinfo");
        } else {
            mp->SetAttribute("submenu", "menu-multiplayer");
        }
    } else if (page_id == "menu-mpinfo"){
        Rocket::Core::Element *item =  menu_page->GetElementById("continue");
        if (show_mutiplayer_info) {
            item->SetProperty("display", "block");
            item->SetClass("disabled", false);
            show_mutiplayer_info = 0;
        } else {
            item->SetProperty("display", "none");
            item->SetClass("disabled", true);
        }
    } else if (page_id == "menu-multiplayer-lobby"){
        Rocket::Core::Element * textarea =  menu_page->GetElementById("chat");
        textarea->SetAttribute("value", "");
        Rocket::Core::Element * input =  menu_page->GetElementById("message-input");
        input->SetAttribute("value", "");
    } else if (page_id == "menu-start") {
        if (dnGamepadConnected()) {
            Rocket::Core::Element * start_text =  menu_page->GetElementById("start");
            start_text->SetInnerRML("PRESS \"A\"");
        }
    }
}

void GUI::InitLoadPage(Rocket::Core::ElementDocument *menu_page) {
    for (int i = 9; i >= 0; i--) {
        Rocket::Core::Element *menu_item;
        char id[20];
        sprintf(id, "slot%d", i);
        menu_item = menu_page->GetElementById(id);
        if (menu_item != NULL) {
            struct savehead saveh;
            if (loadpheader((char)i, &saveh) == 0) {
                menu_item->SetInnerRML(saveh.name);
                menu_item->SetClass("empty", false);
                menu_item->RemoveAttribute("noanim");
            } else {
                menu_item->SetInnerRML("EMPTY");
                menu_item->SetClass("empty", true);
                menu_item->SetAttribute("noanim", true);
            }
        }
    }
    m_menu->HighlightItem(menu_page, "slot1");
    m_menu->HighlightItem(menu_page, "slot0"); // load menu bug workaround
}

void GUI::InitMouseSetupPage(Rocket::Core::ElementDocument *menu_page) {
    float sens = (float)clamp(dnGetMouseSensitivity()/65536.0, 0.0, 1.0);
    m_menu->SetRangeValue(m_menu->GetMenuItem(menu_page, "mouse-sens"), sens);
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "invert-y-axis"),
            ud.mouseflip ? "invert-y-on" : "invert-y-off",
            false);    
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "lock-y-axis"),
                           myaimmode ? "lock-y-off" : "lock-y-on",
                           false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(menu_page, "x-mouse-scale"), (float)xmousescale);
    m_menu->SetRangeValue(m_menu->GetMenuItem(menu_page, "y-mouse-scale"), (float)ymousescale);

}

void GUI::InitGamepadSetupPage(Rocket::Core::ElementDocument *menu_page) {
    Rocket::Core::Element *rumble = menu_page->GetElementById("gamepad-rumble");
    
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "invert-y-axis"),
                           ud.mouseflip ? "invert-y-on" : "invert-y-off",
                           false);
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "lock-y-axis"),
                           myaimmode ? "lock-y-off" : "lock-y-on",
                           false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(menu_page, "x-gamepad-scale"), (float)xgamepadscale);
    m_menu->SetRangeValue(m_menu->GetMenuItem(menu_page, "y-gamepad-scale"), (float)ygamepadscale);
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "gamepad-move-stick"),
                           movestickleft ? "gamepad-move-stick-left" : "gamepad-move-stick-right",
                           false);
    m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "gamepad-rumble"),
                           gamepadrumble ? "gamepad-rumble-on" : "gamepad-rumble-off",
                           false);
    
    if ( rumble != NULL) {
        if (dnCanVibrate()) {
            rumble->SetClass("disabled", false);
            m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "gamepad-rumble"),
                                   gamepadrumble ? "gamepad-rumble-on" : "gamepad-rumble-off",
                                   false);
        } else {
            rumble->SetClass("disabled", true);
            m_menu->ActivateOption(m_menu->GetMenuItem(menu_page, "gamepad-rumble"),
                                  "gamepad-rumble-off", false);
        }
    }
}


void GUI::InitVideoOptionsPage(Rocket::Core::ElementDocument *page) {
    char buf[10];
    VideoMode vm;
    dnGetCurrentVideoMode(&vm);
    SetChosenMode(&vm);
    UpdateApplyStatus();
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "texture-filter"), gltexfiltermode < 3 ? "retro" : "smooth", false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "gamma"), (float)dnGetBrightness(), false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "vertical-sync"), ud.vsync ? "vsync-on" : "vsync-off", false);
    int fps_max = clamp((int)((ud.fps_max+5)/10)*10, 30, 180);
    sprintf(buf, "fps-%d", fps_max);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "max-fps"), buf, false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "accurate-lighting"), AccurateLighting ? "accurate-lighting-on" : "accurate-lighting-off", false);

}

void GUI::InitSoundOptionsPage(Rocket::Core::ElementDocument *page) {
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "sound"), SoundToggle ? "sound-on" : "sound-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "music"), MusicToggle ? "music-on" : "music-off", false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "sound-volume"), (float)FXVolume, false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "music-volume"), (float)MusicVolume, false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "duke-talk"), VoiceToggle ? "talk-on" : "talk-off", false);
}

void GUI::InitGameOptionsPage(Rocket::Core::ElementDocument *page) {
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "crosshair"), ud.crosshair ? "crosshair-on" : "crosshair-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "level-stats"), ud.levelstats ? "stats-on" : "stats-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "auto-aiming"), AutoAim ? "autoaim-on" : "autoaim-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "run-key-style"), ud.runkey_mode ? "runkey-classic" : "runkey-modern", false);
    char b[20];
    sprintf(b, "autoswitch-%d", clamp((int)ud.weaponswitch, 0, 3));
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "weapon-switch"), b, false);
    sprintf(b, "statusbar-%d", clamp(((int)ud.screen_size)/4, 0, 2));
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "statusbar"), b, false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "messages"), ud.fta_on ? "messages-on" : "messages-off", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "mature-content"), ud.lockout ? "mature-content-off" : "mature-content-on", false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "opponents-weapon"), ShowOpponentWeapons ? "opponents-weapon-on" : "opponents-weapon-off", false);
}


void GUI::InitLobbyFilterPage(Rocket::Core::ElementDocument *page) {
    char b[20];
    sprintf(b, "gamemode-filter-%d", clamp((int)lb.gamemode, 0, 3));
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "gamemode-filter"), b, false);
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "show-full"), lb.show_full ? "show-full-on" : "show-full-off", false);
    sprintf(b, "maps-filter-%d", clamp((int)lb.maps, 0, 3));
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "maps-filter"), b, false);
}

void GUI::InitKeysSetupPage(Rocket::Core::ElementDocument *page) {
    Rocket::Core::Element *hint = page->GetElementById("hint");
    for (int i = 0, n = m_menu->GetNumMenuItems(page); i != n; i++) {
        Rocket::Core::Element *menu_item = m_menu->GetMenuItem(page, i);
        const Rocket::Core::String& gamefunc = menu_item->GetId();
        int func_num = CONFIG_FunctionNameToNum(gamefunc.CString());
        if (func_num == -1) {
            m_menu->SetKeyChooserValue(menu_item, 0, "???", "???");
            m_menu->SetKeyChooserValue(menu_item, 1, "???", "???");
        } else {
            dnKey key0 = dnGetFunctionBinding(func_num, 0);
            dnKey key1 = dnGetFunctionBinding(func_num, 1);
            const char *key0_name = dnGetKeyName(key0);
            const char *key1_name = dnGetKeyName(key1);
            m_menu->SetKeyChooserValue(menu_item, 0, key0_name, key0_name);
            m_menu->SetKeyChooserValue(menu_item, 1, key1_name, key1_name);
        }
    }
    if (dnGamepadConnected()) {
        hint->SetInnerRML("Press BACK to reset bindings");
    } else {
        hint->SetInnerRML("Press F8 to reset bindings");
    }

}


void GUI::InitUserMapsPage(const char * page_id) {
    Rocket::Core::ElementDocument *menu_page = m_context->GetDocument("menu-usermaps");
    CACHE1D_FIND_REC * files = dnGetMapsList();
    dnUnsetWorkshopMap();
    Rocket::Core::Element *menu = menu_page->GetElementById("menu");
    bool haveFiles = false;
	Rocket::Core::String firstItem;
    
    int num = CSTEAM_NumWorkshopItems();
	bool workshop_header_added = false;
    menu->SetInnerRML("");
    if (num > 0) {
        for (int i=0; i < num; i++) {
            workshop_item_t item;
            CSTEAM_GetWorkshopItemByIndex(i, &item);
            if (strstr(item.tags, "Singleplayer") == NULL)
                continue;
			if (!workshop_header_added) {
				Rocket::Core::Element *title = new Rocket::Core::Element("div");
				title->SetInnerRML("WORKSHOP MAPS (SINGLEPLAYER)");
				title->SetClass("listhdr", true);
				title->SetAttribute("noanim", "");
				menu->AppendChild(title);
				m_menu->SetupMenuItem(title);
				workshop_header_added = true;
			}
            Rocket::Core::Element * record = new Rocket::Core::Element("div");
            record->SetProperty("item-id", va("%llu", item.item_id));
            std::string item_title(item.title);
            encode(item_title);
            record->SetInnerRML(va("%s (%s)", item_title.c_str(), item.itemname));
            record->SetId(item.itemname);
            record->SetAttribute("command", "load-workshopitem");
            menu->AppendChild(record);
            m_menu->SetupMenuItem(record);
			haveFiles = true;
			if (firstItem == "") {
				firstItem = record->GetId();
			}
        }
    }
    
    if (files) {
        if (num == 0)
            menu->SetInnerRML("");
        haveFiles = true;
        Rocket::Core::Element *title = new Rocket::Core::Element("div");
        title->SetInnerRML("USER MAPS");
        title->SetClass("listhdr", true);
        title->SetAttribute("noanim", "");
        menu->AppendChild(title);
        m_menu->SetupMenuItem(title);
        while (files) {
            Rocket::Core::Element * record = new Rocket::Core::Element("div");
            record->SetProperty("map-name", files->name);
            record->SetInnerRML(Bstrlwr(files->name));
            record->SetId(files->name);
			if (firstItem == "") {
				firstItem = record->GetId();
			}
            record->SetAttribute("command", "load-usermap");
            menu->AppendChild(record);
            files = files->next;
            m_menu->SetupMenuItem(record);
        }
    }
    if (haveFiles) {
        m_menu->HighlightItem(menu_page, firstItem);
	} else {
		Rocket::Core::Element *title = new Rocket::Core::Element("div");
		title->SetInnerRML("No maps found");
		title->SetClass("empty", true);
		title->SetAttribute("noanim", "");
		title->SetId("emptyhdr");
		menu->AppendChild(title);
		m_menu->SetupMenuItem(title);
		m_menu->HighlightItem(menu_page, "emptyhdr");
	}
}

void GUI::InitMpMapsPage(Rocket::Core::ElementDocument *menu_page) {
    Rocket::Core::Element *menu = menu_page->GetElementById("menu");
    Rocket::Core::Element *first = NULL;
    dnUnsetWorkshopMap();
    menu->SetInnerRML("");

    /* go through all the volumes */
    for (int volnum = 0; volnum < sizeof(volume_names)/sizeof(volume_names[0]); volnum++) {
        const char *volname = volume_names[volnum];
        if (volname[0] == 0) {
            break;
        }
        /* add non-functional menu item to designate volume name */
        Rocket::Core::Element *record = new Rocket::Core::Element("div");
        record->SetInnerRML(va("EPISODE %d: %s", volnum+1, volname));
//        record->SetClass("empty", true);
        record->SetClass("listhdr", true);
        record->SetAttribute("noanim", "");
        menu->AppendChild(record);
        m_menu->SetupMenuItem(record);
        /* go through all the levels of the current volume */
        for (int levnum = 0; levnum < 11; levnum++) {
            const char *levname = level_names[levnum+volnum*11];
            if (levname[0] == 0) {
                break;
            }
            if (volnum == 0 && levnum == 7) {
                continue; /* skip e1m8 since that means to load a user map */
            }
            
            if (volnum == 0 && levnum > 7) {
                continue; /* skipping non-existing levels
                          E1L9.map VOID ZONE
                          E1L10.map ROACH CONDO
                          E1L11.mapANTIPROFIT
                           */
            }
            Rocket::Core::Element *record = new Rocket::Core::Element("div");
            record->SetInnerRML(va("E%dM%d - %s", volnum+1, levnum+1, levname));
            record->SetClass("builtin", true);
            record->SetId(va("level-e%dm%d", volnum+1, levnum+1));
            record->SetAttribute("command", "choose-mp-level");
            record->SetAttribute("mapname", va("E%dM%d", volnum+1, levnum+1));
            if (first != NULL) {
                first = record;
            }
            menu->AppendChild(record);
            m_menu->SetupMenuItem(record);
        }
    }
        int num = CSTEAM_NumWorkshopItems();
        if (num > 0) {
            Rocket::Core::Element *title = new Rocket::Core::Element("div");
            title->SetInnerRML("WORKSHOP MAPS (MULTIPLAYER)");
            title->SetClass("listhdr", true);
            title->SetAttribute("noanim", "");
            menu->AppendChild(title);
            m_menu->SetupMenuItem(title);
            for (int i=0; i < num; i++) {
                workshop_item_t item;
                CSTEAM_GetWorkshopItemByIndex(i, &item);
                if (strstr(item.tags, "Deathmatch") == NULL && strstr(item.tags, "Coop") == NULL)
                    continue;
                Rocket::Core::Element * record = new Rocket::Core::Element("div");
                record->SetAttribute("item-id", va("%llu", item.item_id));
                std::string item_title(item.title);
                encode(item_title);
                record->SetId(item.itemname);
                record->SetClass("workshop", true);
                record->SetAttribute("command", "choose-mp-level");
                record->SetAttribute("mapname", item.itemname);
                record->SetInnerRML(va("%s (%s)", item_title.c_str(), item.itemname));
                menu->AppendChild(record);
                m_menu->SetupMenuItem(record);
            }
        }
    
    bool usermap_header_added = false;
    CACHE1D_FIND_REC * files = dnGetMapsList();
    if (files) {
        while (files) {
            if (!usermap_header_added) {
                usermap_header_added = true;
                Rocket::Core::Element *record = new Rocket::Core::Element("div");
                record->SetInnerRML("USER MAPS");
                record->SetClass("listhdr", true);
                record->SetAttribute("noanim", "");
                menu->AppendChild(record);
                m_menu->SetupMenuItem(record);

            }
            Rocket::Core::Element * record = new Rocket::Core::Element("div");
            record->SetClass("usermap", true);
            record->SetAttribute("command", "choose-mp-level");
            record->SetAttribute("mapname", files->name);
            record->SetInnerRML(Bstrlwr(files->name));
            menu->AppendChild(record);
            files = files->next;
            m_menu->SetupMenuItem(record);
        }
    }
    m_menu->HighlightItem(menu_page, "level-e1m1");
}


void GUI::InitLobbyList(Rocket::Core::ElementDocument *menu_page) {
    UpdateLobbyList();
    Rocket::Core::Element *menu = menu_page->GetElementById("menu");
    Rocket::Core::Element *hint = menu_page->GetElementById("hint");
    menu->SetInnerRML("<div id='stub' noanim class='empty'>Updating...</div>");
    m_menu->HighlightItem(menu_page, "stub");
    if (dnGamepadConnected()) {
        hint->SetInnerRML("Press X to refresh");
    } else {
        hint->SetInnerRML("Press SPACE to refresh");
    }
}


void GUI::DidCloseMenuPage(Rocket::Core::ElementDocument *menu_page) {
    const Rocket::Core::String& page_id = menu_page->GetId();
    
    if (m_action_to_confirm != NULL && m_action_to_confirm->doc->GetId() == page_id) {
        m_action_to_confirm->OnClose();
    }
    
	if (page_id == "menu-video") {
    } else if (page_id == "menu-keys-setup") {
        /*
        dnResetMouseKeyBindings();
         */
        ApplyNewKeysSetup(menu_page);
         
    } else if (page_id == "menu-mouse-setup") {
        float sens = m_menu->GetRangeValue(m_menu->GetMenuItem(menu_page, "mouse-sens"));
        int sens_int = clampi((int) (65536.0*sens), 0, 65535);
        dnSetMouseSensitivity(sens_int);
        bool invert_y = m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "invert-y-axis"))->GetId() == "invert-y-on";
        ud.mouseflip = invert_y ? 1 : 0;
        bool lock_y = m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "lock-y-axis"))->GetId() == "lock-y-on";
        if (lock_y) {
            ps[myconnectindex].horiz = 100;
            AutoAim = 1;
            ps[myconnectindex].auto_aim = AutoAim;
            myaimmode = 0;
        } else {
            myaimmode = 1;
        }
        myaimmode = lock_y ? 0 : 1;
    } else if (page_id == "menu-multiplayer-lobby") {
    } else if (page_id == "menu-gamepad-setup") {
        bool invert_y = m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "invert-y-axis"))->GetId() == "invert-y-on";
        ud.mouseflip = invert_y ? 1 : 0;
        bool lock_y = m_menu->GetActiveOption(m_menu->GetMenuItem(menu_page, "lock-y-axis"))->GetId() == "lock-y-on";
        if (lock_y) {
            ps[myconnectindex].horiz = 100;
            AutoAim = 1;
            ps[myconnectindex].auto_aim = AutoAim;
            myaimmode = 0;
        } else {
            myaimmode = 1;
        }
        myaimmode = lock_y ? 0 : 1;

    }
}

void GUI::ApplyVideoMode(Rocket::Core::ElementDocument *menu_page) {
    
    struct ApplyVideoModeAction: public ConfirmableAction {
        VideoMode new_mode;
        VideoMode backup_mode;
        ApplyVideoModeAction(Rocket::Core::ElementDocument *back_page):ConfirmableAction(back_page){}
        virtual void Yes() {
            Sys_DPrintf("Apply Video Mode\n");
            SaveVideoMode(&new_mode);
        }
        virtual void No() {
            Sys_DPrintf("Restore Video Mode\n");
            dnChangeVideoMode(&backup_mode);
            SaveVideoMode(&backup_mode);
        }
    };
    
    Rocket::Core::ElementDocument *doc = menu_page;
    dnGetCurrentVideoMode(&m_backup_video_mode);
    ReadChosenMode(&m_new_video_mode);
    dnChangeVideoMode(&m_new_video_mode);
//    UpdateApplyStatus();
    /* restore mouse pointer */
//    SDL_WM_GrabInput(SDL_GRAB_OFF);
//    SDL_ShowCursor(1);

    /*
    if (1) {
        ApplyVideoModeAction *action = new ApplyVideoModeAction(m_context->GetDocument("menu-options"));
        action->new_mode = vm;
        action->backup_mode = backup_mode;
        ShowConfirmation(action, "video-confirm", "no");
    } else {
        dnGetCurrentVideoMode(&vm);
        SaveVideoMode(&vm);
    }
     */
    vscrn();
    m_menu->ShowDocument(m_context, "video-confirm");
    m_menu->HighlightItem(m_context, "video-confirm", "no");
}

void GUI::ApplyNewKeysSetup(Rocket::Core::ElementDocument *menu_page) {
    dnResetBindings();
    for (int i = 0, n = m_menu->GetNumMenuItems(menu_page); i != n; i++) {
        Rocket::Core::Element *menu_item = m_menu->GetMenuItem(menu_page, i);
        const Rocket::Core::String& function_name = menu_item->GetId();
        const Rocket::Core::String& key0_id = m_menu->GetKeyChooserValue(menu_item, 0);
        const Rocket::Core::String& key1_id = m_menu->GetKeyChooserValue(menu_item, 1);

        AssignFunctionKey(function_name, key0_id, 0);
        AssignFunctionKey(function_name, key1_id, 1);
    }
    dnApplyBindings();
}

void GUI::AssignFunctionKey(Rocket::Core::String const & function_name, Rocket::Core::String const & key_id, int slot) {
    int function_num = CONFIG_FunctionNameToNum(function_name.CString());
    dnKey key = dnGetKeyByName(key_id.CString());
    dnBindFunction(function_num, slot, key);
}

static
bool ParseVideoMode(const char *str, VideoMode *mode) {
	return sscanf(str, "%dx%d@%d", &mode->width, &mode->height, &mode->bpp) == 3;
}

void GUI::ReadChosenMode(VideoMode *mode) {
	assert(mode != NULL);
	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-video");
	assert(doc!=NULL);
	Rocket::Core::Element *mode_item = m_menu->GetMenuItem(doc, "video-mode");
	Rocket::Core::Element *fs_item = m_menu->GetMenuItem(doc, "fullscreen-mode");
	assert(mode_item!=NULL && fs_item!=NULL);
	Rocket::Core::Element *mode_option = m_menu->GetActiveOption(mode_item);
	Rocket::Core::Element *fs_option = m_menu->GetActiveOption(fs_item);
	assert(mode_option!=NULL && fs_option!=NULL);
	bool mode_ok = ParseVideoMode(mode_option->GetId().CString(), mode);
	assert(mode_ok);
	mode->fullscreen = (fs_option->GetId() == "fullscreen-on") ? 1 : 0;
}

void GUI::SetChosenMode(const VideoMode *mode) {
	assert(mode != NULL);
	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-video");
	assert(doc!=NULL);
	Rocket::Core::Element *mode_item = m_menu->GetMenuItem(doc, "video-mode");
	Rocket::Core::Element *fs_item = m_menu->GetMenuItem(doc, "fullscreen-mode");
	assert(mode_item!=NULL && fs_item!=NULL);

	char mode_id[40];
	sprintf(mode_id, "%dx%d@%d", mode->width, mode->height, mode->bpp);
	const char *fs_id = mode->fullscreen ? "fullscreen-on" : "fullscreen-off";
	m_menu->ActivateOption(mode_item, mode_id);
	m_menu->ActivateOption(fs_item, fs_id);
}

void GUI::UpdateApplyStatus() {
	VideoMode chosen_mode, current_mode;
	ReadChosenMode(&chosen_mode);
	dnGetCurrentVideoMode(&current_mode);
	Rocket::Core::ElementDocument *doc = m_context->GetDocument("menu-video");
	Rocket::Core::Element *apply_item = m_menu->GetMenuItem(doc, "apply");
    m_need_apply_video_mode = !(chosen_mode == current_mode);
	if (apply_item != NULL) {
		bool apply_disabled = chosen_mode == current_mode;
		apply_item->SetClass("disabled", apply_disabled);
		if (apply_disabled && m_menu->GetHighlightedItem(doc) == apply_item) {
			m_menu->HighlightNextItem(doc);
		}
	}
}

void GUI::DidChangeOptionValue(Rocket::Core::Element *menu_item, Rocket::Core::Element *new_value) {
	Rocket::Core::String item_id(menu_item->GetId());
	Rocket::Core::String value_id(new_value->GetId());
	if (item_id == "video-mode" || item_id == "fullscreen-mode") {
		UpdateApplyStatus();
	}
	if (item_id == "texture-filter") {
		if (value_id == "retro") {
			gltexfiltermode = 2;
			gltexapplyprops();
		} else {
			gltexfiltermode = 5;
			gltexapplyprops();
		}
    } else if (item_id == "accurate-lighting") {
        AccurateLighting = (value_id == "accurate-lighting-on");
        if (AccurateLighting) {
            r_usenewshading = 2;
            r_usetileshades = 1;
        } else {
            r_usenewshading = 0;
            r_usetileshades = 0;
        }
	} else if (item_id == "sound") {
		dnEnableSound( value_id == "sound-on" ? 1 : 0 );
	} else if (item_id == "music") {
		dnEnableMusic( value_id == "music-on" ? 1 : 0 );
	} else if (item_id == "duke-talk") {
		dnEnableVoice( value_id == "talk-on" ? 1 : 0 );
	} else if (item_id == "crosshair") {
		ud.crosshair = ( value_id == "crosshair-on" ? 1 : 0 );
	} else if (item_id == "level-stats") {
		ud.levelstats = ( value_id == "stats-on" ? 1 : 0 );
    } else if (item_id == "gamepad-rumble") {
		gamepadrumble = ( value_id == "gamepad-rumble-on" ? 1 : 0 );
    } else if (item_id == "gamepad-move-stick") {
		movestickleft = ( value_id == "gamepad-move-stick-left" ? 1 : 0 );
	} else if (item_id == "auto-aiming") {
        if (ud.multimode < 2) {
            AutoAim = ( value_id == "autoaim-on" ? 1 : 0 );
            ps[myconnectindex].auto_aim = AutoAim;
        }
	} else if (item_id == "run-key-style") {
		ud.runkey_mode = ( value_id == "runkey-classic" ? 1 : 0 );
	} else if (item_id == "mature-content") {
        ud.lockout = ( value_id == "mature-content-on" ? 0 : 1 );
    } else if (item_id == "messages") {
        ud.fta_on = ( value_id == "messages-on" ? 1 : 0 );
    } else if (item_id == "opponents-weapon") {
        ud.showweapons = ShowOpponentWeapons = ( value_id == "opponents-weapon-on" ? 1 : 0 );
    } else if (item_id == "weapon-switch") {
        if( ud.multimode < 2) {
            int v = 3;
            sscanf(value_id.CString(), "autoswitch-%d", &v);
            ud.weaponswitch = v;
            ps[myconnectindex].weaponswitch = ud.weaponswitch;
        }
	} else if (item_id == "statusbar") {
		int v = 2;
		sscanf(value_id.CString(), "statusbar-%d", &v);
		ud.screen_size = v*4;
		ud.statusbarscale = 100;
		vscrn();
	} else if (item_id == "max-fps") {
        int fps;
        if (sscanf(value_id.CString(), "fps-%d", &fps) == 1) {
            ud.fps_max = fps;
        }
	} else if (item_id == "vertical-sync") {
		int a = ud.vsync ? 1 : 0;
		int b = value_id == "vsync-on" ? 1 : 0;
		m_need_apply_vsync = a != b;
	} else if (item_id == "gamemode") {
        Rocket::Core::ElementDocument *menu_page = m_context->GetDocument("menu-multiplayer-create");
        Rocket::Core::Element *ff_item = m_menu->GetMenuItem(menu_page, "friendlyfire");
        Rocket::Core::Element *skill_item = m_menu->GetMenuItem(menu_page, "skill");
        Rocket::Core::Element *timelimit_item = m_menu->GetMenuItem(menu_page, "timelimit");
        Rocket::Core::Element *fraglimit_item = m_menu->GetMenuItem(menu_page, "fraglimit");
        
        bool enable_ff_item = value_id != "mode-0"; /* dm */
        bool enable_skill_item = value_id == "mode-2"; /* coop */
        
        ff_item->SetClass("disabled", !enable_ff_item);
        skill_item->SetClass("disabled", !enable_skill_item);
        timelimit_item->SetClass("disabled", enable_ff_item);
        fraglimit_item->SetClass("disabled", enable_ff_item);
    } else if (item_id == "gamemode-filter") {
        int v = 3;
        sscanf(value_id.CString(), "gamemode-filter-%d", &v);
        lb.gamemode = v;
    } else if (item_id == "show-full") {
        lb.show_full = value_id == "show-full-on" ? 1 : 0;
    } else if (item_id == "maps-filter") {
        int v = 0;
        sscanf(value_id.CString(), "maps-filter-%d", &v);
        lb.maps = v;
    }
}

void GUI::DidChangeRangeValue(Rocket::Core::Element *menu_item, float new_value) {
	Rocket::Core::String item_id(menu_item->GetId());
	if (item_id == "gamma") {
		m_setBrightness = clamp((int)new_value, 0, 63);
	} else if (item_id == "sound-volume") {
		dnSetSoundVolume(clamp((int)new_value, 0, 255));
	} else if (item_id == "music-volume") {
		dnSetMusicVolume(clamp((int)new_value, 0, 255));
	} else if (item_id == "x-mouse-scale") {
        xmousescale = clamp((int)new_value, 1, 20);
    } else if (item_id == "y-mouse-scale") {
        ymousescale = clamp((int)new_value, 1, 20);
	} else if (item_id == "x-gamepad-scale") {
        xgamepadscale = clamp((int)new_value, 1, 20);
    } else if (item_id == "y-gamepad-scale") {
        ygamepadscale = clamp((int)new_value, 1, 20);
    }

}

void GUI::DidRequestKey(Rocket::Core::Element *menu_item, int slot) {
    m_waiting_for_key = true;
    m_pressed_key = SDL_SCANCODE_UNKNOWN;
    m_waiting_menu_item = menu_item;
    m_waiting_slot = slot;
    m_context->GetDocument("key-prompt")->Show();
}

static
Rocket::Core::String GetEpisodeName(Rocket::Core::Context *context, int episode) {
    Rocket::Core::String r;
    char buffer[20];
    sprintf(buffer, "episode-%d", episode);
    Rocket::Core::ElementDocument *doc = context->GetDocument("menu-episodes");
    Rocket::Core::Element *e = doc->GetElementById(buffer);
    if (e != NULL) {
        e->GetInnerRML(r);
    }
    return r;
}

static
Rocket::Core::String GetSkillName(Rocket::Core::Context *context, int skill) {
    Rocket::Core::String r;
    char buffer[20];
    sprintf(buffer, "skill-%d", skill);
    Rocket::Core::ElementDocument *doc = context->GetDocument("menu-skill");
    Rocket::Core::Element *e = doc->GetElementById(buffer);
    if (e != NULL) {
        e->GetInnerRML(r);
    }
    return r;
}

void GUI::DidActivateItem(Rocket::Core::Element *menu_item) {
    int slot;
    Rocket::Core::String item_id = menu_item->GetId();
    if (sscanf(item_id.CString(), "slot%d", &slot) == 1) {
        Rocket::Core::ElementDocument *doc = menu_item->GetOwnerDocument();
        Rocket::Core::Element *thumbnail = doc->GetElementById("thumbnail");
        Rocket::Core::Element *skill = doc->GetElementById("skill");
        Rocket::Core::Element *episode = doc->GetElementById("episode");
        Rocket::Core::Element *level = doc->GetElementById("level");
        
        if (slot >= 0  && slot < 10) {
            char thumb_rml[50];
            char level_rml[50];
            struct savehead saveh;
            if (loadpheader(slot, &saveh) == 0 && !menu_item->IsClassSet("empty")) {
                sprintf(thumb_rml, "<img src=\"tile:%d?%d\"></img>", TILE_LOADSHOT, SDL_GetTicks());
                skill->SetInnerRML(GetSkillName(m_context, saveh.plrskl));
                episode->SetInnerRML(dnGetEpisodeName(saveh.volnum));
                sprintf(level_rml, "%s", /*(int)saveh.levnum+1,*/ dnGetLevelName(saveh.volnum, (int)saveh.levnum));
            } else {
                sprintf(thumb_rml, "<img src=\"assets/placeholder.tga\"></img>");
                skill->SetInnerRML("");
                episode->SetInnerRML("");
                strcpy(level_rml, "");
            }
            thumbnail->SetInnerRML(thumb_rml);
            level->SetInnerRML(level_rml);
        }
    } else if (item_id == "message") {
//        if (dnGamepadConnected())
//            CSTEAM_ShowGamepadTextInput("", 140);
    }
}

bool GUI::WillChangeOptionValue(Rocket::Core::Element *menu_item, int direction) {
    Rocket::Core::String item_id = menu_item->GetId();
    if (item_id == "creategame-level") {
        m_menu->ShowDocument(m_context, "menu-multiplayer-maps");
        return false;
    }
    return true;
}

void GUI::DidClearKeyChooserValue(Rocket::Core::Element *menu_item, int slot) {
    AssignFunctionKey(menu_item->GetId(), "", slot);
}

void GUI::DidClearItem(Rocket::Core::Element *menu_item) {
	if (menu_item->IsClassSet("game-slot")) {
		int slot;
		struct RemoveGameAction: public ConfirmableAction {
			Rocket::Core::Element *element;
			int slot;
			RemoveGameAction(Rocket::Core::ElementDocument *page, Rocket::Core::Element *element, int slot):ConfirmableAction(page), element(element), slot(slot){}
			virtual void Yes() {
				Rocket::Core::ElementDocument *doc = element->GetOwnerDocument();
				Rocket::Core::Element *thumbnail = doc->GetElementById("thumbnail");
				Rocket::Core::Element *skill = doc->GetElementById("skill");
				Rocket::Core::Element *episode = doc->GetElementById("episode");
				Rocket::Core::Element *level = doc->GetElementById("level");
				Rocket::Core::String test;
				skill->SetInnerRML("");
				episode->SetInnerRML("");
				thumbnail->SetInnerRML("<img src=\"assets/placeholder.tga\"></img>");
				level->SetInnerRML("");
				element->SetInnerRML("EMPTY");
				element->SetClass("empty", true);
				char filename[15];
				sprintf(filename, "game%d_%d.sav", slot, dnGetAddonId());
				CSTEAM_DeleteCloudFile(filename);
                unlink(filename);
			}
		};
		if (sscanf(menu_item->GetId().CString(), "slot%d", &slot) == 1) {
			if (!menu_item->IsClassSet("empty")) {
				Rocket::Core::ElementDocument *page = menu_item->GetOwnerDocument();
				ShowConfirmation(new RemoveGameAction(page, menu_item, slot), "yesno", "Remove saved game?");
			}
		}
	}
}

void GUI::DidRefreshItem(Rocket::Core::Element *menu_item) {
    Rocket::Core::ElementDocument *doc = menu_item->GetOwnerDocument();
    if (doc->GetElementById("menu-multiplayer-join") != NULL) {
        InitLobbyList(doc);
    }
}

void GUI::DidResetItem(Rocket::Core::Element *menu_item) {
    Rocket::Core::ElementDocument *doc = menu_item->GetOwnerDocument();
    if (doc->GetElementById("menu-keys-setup") != NULL) {
        dnResetBindings();
        InitKeysSetupPage(doc);
    }
}



void GUI::ShowMenuByID(const char * menu_id) {
    menu_to_open = menu_id;
    ps[myconnectindex].gm |= MODE_MENU;
}

void GUI::ShowSaveMenu() {
    ShowMenuByID("menu-save");
}

void GUI::ShowLoadMenu() {
    ShowMenuByID("menu-load");
}

void GUI::ShowHelpMenu() {
    ShowMenuByID("menu-help");
}

void GUI::ShowSoundMenu() {
    ShowMenuByID("menu-sound");
}

void GUI::ShowGameOptionsMenu() {
     ShowMenuByID("menu-game-options");
}

void GUI::ShowVideoSettingsMenu() {
    ShowMenuByID("menu-video");
}

void GUI::ShowQuitConfirmation() {
    ShowMenuByID("quit-confirm");
}

