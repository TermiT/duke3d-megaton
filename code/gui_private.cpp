//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "gui_private.h"
#include <assert.h>
#include <math.h>
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
//#include "duke3d.h"
//#include "glguard.h"

extern "C" {
#include "types.h"
#include "gamedefs.h"
#include "function.h"
#include "config.h"
#include "build.h"
#include "duke3d.h"
}

#ifndef _WIN32
#include <unistd.h>
#endif

struct ConfirmableAction {
    Rocket::Core::ElementDocument *back_page;
    virtual void Yes() {};
    virtual void No() {};
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
	font_names[0] = "DukeFont.ttf";

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
	LoadDocument("data/keys.rml");
    LoadDocument("data/keyprompt.rml");
    LoadDocument("data/yesno.rml");
    LoadDocument("data/videoconfirm.rml");
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

GUI::GUI(int width, int height):m_enabled(false),m_width(width),m_height(height),m_enabled_for_current_frame(false),m_waiting_for_key(false),m_action_to_confirm(NULL),m_need_apply_video_mode(false),m_show_press_enter(true),m_draw_strips(false) {
    
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

void GUI::Render() {
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
	}
}

void GUI::Enable(bool v) {
	if (v != m_enabled) {
		KB_FlushKeyboardQueue();
		KB_ClearKeysDown();
		dnResetMouse();
		m_enabled = v;
		if (m_enabled) {
            printf("Menu enabled\n");
            if (dnGameModeSet()) {
                m_menu->ShowDocument(m_context, "menu-ingame", false);
            } else {
                m_menu->ShowDocument(m_context, m_show_press_enter ? "menu-start" : "menu-main", false);
            }
			if (dnGameModeSet()) {
				m_context->GetDocument("menu-bg")->Hide();
			} else {
				Rocket::Core::ElementDocument *menubg = m_context->GetDocument("menu-bg");
				menubg->Show();
			}
            ResetMouse();
            /*
			if (!m_menu->GetCurrentPage(m_context)->IsVisible()) {
				m_menu->GetCurrentPage(m_context)->Show();
			}
			*/
//			SDL_GetMouseState(&m_saved_mouse_x, &m_saved_mouse_y);
//			SDL_WM_GrabInput(SDL_GRAB_OFF);
//			SDL_ShowCursor(1);
			//m_context->ShowMouseCursor(true);
            
		} else {
//			SDL_WarpMouse(m_width/2, m_height/2);
//			SDL_WM_GrabInput(SDL_GRAB_ON);
//			SDL_ShowCursor(0);
			m_context->ShowMouseCursor(false);
			//SDL_WarpMouse(m_saved_mouse_x, m_saved_mouse_y);
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
TranslateRange(SDLKey min, SDLKey max, SDLKey key, Rocket::Core::Input::KeyIdentifier base, Rocket::Core::Input::KeyIdentifier *result) {
	int offset;
	if (in_range(min, max, key, &offset)) {
		*result = (Rocket::Core::Input::KeyIdentifier)(base + offset);
		return true;
	}
	return false;
}

static Rocket::Core::Input::KeyIdentifier
translateKey(SDLKey key) {
	using namespace Rocket::Core::Input;
	KeyIdentifier result;
	if (TranslateRange(SDLK_0, SDLK_9, key, KI_0, &result)) { return result; }
	if (TranslateRange(SDLK_F1, SDLK_F12, key, KI_F1, &result)) { return result; }
	if (TranslateRange(SDLK_a, SDLK_z, key, KI_A, &result)) { return result; }
	if (key == SDLK_CAPSLOCK) { return KI_CAPITAL; }
	if (key == SDLK_TAB) { return KI_TAB; }
	return KI_UNKNOWN;
}

#define KEYDOWN(ev, k) (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == k)

bool GUI::InjectEvent(SDL_Event *ev) {
	bool retval = false;

#if 0
	if (ev->type == SDL_KEYDOWN && ev->key.keysym.sym == SDLK_BACKQUOTE) {
		if (isEnabled()) {
			if (!m_menu->GoBack(m_context)) {
				enable(false);
			}
		} else {
			enable(true);
		}
		return true;
	}
#endif
#if !CLASSIC_MENU
	if (KEYDOWN(ev, SDLK_ESCAPE) || (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == 3 && !m_waiting_for_key)) {
        if (m_waiting_for_key) {
            m_context->GetDocument("key-prompt")->Hide();
            m_waiting_for_key = false;
		} else 	{
			m_draw_strips = false;
			if (!m_menu->GoBack(m_context)) {
				dnHideMenu();
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

        if (m_waiting_for_key) {
            switch (ev->type) {
                case SDL_MOUSEBUTTONDOWN:
                    m_pressed_key = (dnKey)(DNK_MOUSE0+ev->button.button-1);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (m_pressed_key == DNK_MOUSE0+ev->button.button-1) {
                        HideKeyPrompt();
                        const char *id = dnGetKeyName(m_pressed_key);
                        AssignFunctionKey(m_waiting_menu_item->GetId(), id, m_waiting_slot);
                        InitKeysSetupPage(m_context->GetDocument("menu-keys-setup"));
                    }
                    break;
                case SDL_KEYDOWN:
                   m_pressed_key = ev->key.keysym.sym;
                    break;
                case SDL_KEYUP:
                    if (m_pressed_key == ev->key.keysym.sym) {
                        HideKeyPrompt();
                        const char *id = dnGetKeyName(m_pressed_key);
                        AssignFunctionKey(m_waiting_menu_item->GetId(), id, m_waiting_slot);
                        InitKeysSetupPage(m_context->GetDocument("menu-keys-setup"));
                    }
                    break;
                default:
                    break;
            }
        } else {
            switch (ev->type) {
                case SDL_MOUSEMOTION: {
                    m_context->ShowMouseCursor(true);
                    int xrel = ev->motion.xrel;
                    int yrel = ev->motion.yrel;
                    if (abs(xrel) < 200 && abs(yrel) < 200) { // jerk filter
                        m_mouse_x = clamp(m_mouse_x+xrel, 0, m_width);
						m_mouse_y = clamp(m_mouse_y+yrel, 0, m_height == 1200 ? 1080 : m_height);
                        Rocket::Core::Vector2i mpos = TranslateMouse(m_mouse_x, m_mouse_y);
                        m_context->ProcessMouseMove(mpos.x, mpos.y, 0);
                    }
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                    if (ev->button.button < 4) {
                        m_context->ProcessMouseButtonDown(ev->button.button-1, 0);
                    } else if (ev->button.button == 4) {
                        m_context->ProcessMouseWheel(-2, 0);
                    } else if (ev->button.button == 5) {
                        m_context->ProcessMouseWheel(2, 0);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (ev->button.button < 4) {
                        m_context->ProcessMouseButtonUp(ev->button.button-1, 0);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (ev->key.keysym.sym == SDLK_LEFT) {
                        m_menu->SetPreviousItemValue(m_context);
                    } else if (ev->key.keysym.sym == SDLK_RIGHT) {
                        m_menu->SetNextItemValue(m_context);
                    } else if (ev->key.keysym.sym == SDLK_DOWN) {
                        m_menu->HighlightNextItem(m_context);
                    } else if (ev->key.keysym.sym == SDLK_UP) {
                        m_menu->HighlightPreviousItem(m_context);
                    } else if (ev->key.keysym.sym == SDLK_RETURN) {
                        m_menu->DoItemAction(ItemActionEnter, m_context);
                    } else if (ev->key.keysym.sym == SDLK_DELETE || ev->key.keysym.sym == SDLK_BACKSPACE) {
                        m_menu->DoItemAction(ItemActionClear, m_context);
                    } else {
                        retval = m_context->ProcessKeyDown(translateKey(ev->key.keysym.sym), 0);
                    }
                    break;
                case SDL_KEYUP:
                    retval = m_context->ProcessKeyUp(translateKey(ev->key.keysym.sym), 0);
                    break;
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
	int ny = (int)(y/m_menu_scale);
	return Rocket::Core::Vector2i(nx, ny);
}

void GUI::EnableForCurrentFrame() {
	m_enabled_for_current_frame = true;
}

static
void NewGame(int skill, int episode, int level) {
	GameDesc gamedesc;

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

static
void SaveVideoMode(VideoMode *vm) {
	/* update config */
	ScreenWidth = vm->width;
	ScreenHeight = vm->height;
	ScreenBPP = vm->bpp;
	ScreenMode = vm->fullscreen;
}

void GUI::ReadChosenSkillAndEpisode(int *pskill, int *pepisode) {
	Rocket::Core::Element *skill = m_menu->GetHighlightedItem(m_context, "menu-skill");
    assert(skill != NULL);
    sscanf(skill->GetId().CString(), "skill-%d", pskill);
    if (dnGetAddonId() != 0) {
        *pepisode = dnGetAddonEpisode();
        return;
    } else {
        Rocket::Core::Element *episode = m_menu->GetHighlightedItem(m_context, "menu-episodes");
        assert(episode != NULL);
        sscanf(episode->GetId().CString(), "episode-%d", pepisode);
        return;
    }
    assert(false);
}

void GUI::DoCommand(Rocket::Core::Element *element, const Rocket::Core::String& command) {
//	printf("[GUI ] Command: %s\n", command.CString());
	if (command == "game-start") {
		int skill, episode;
		ReadChosenSkillAndEpisode(&skill, &episode);
		Enable(false);
        ps[myconnectindex].gm &= ~MODE_MENU;
		m_menu->ShowDocument(m_context, "menu-ingame", false);
		NewGame(skill, episode, 0);
	} else if (command == "game-quit") {
        QuitGameCommand(element);
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
            m_action_to_confirm->Yes();
            if (m_action_to_confirm->back_page != NULL) {
                m_action_to_confirm->back_page->Hide();
            }
            SetActionToConfirm(NULL);
            m_menu->GoBack(m_context);
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
    }
}

void GUI::QuitToTitleCommand(Rocket::Core::Element *element) {

    struct QuitToTitleAction: public ConfirmableAction {
        Rocket::Core::Context *context;
        RocketMenuPlugin *menu;
        QuitToTitleAction(Rocket::Core::ElementDocument *back_page, Rocket::Core::Context *context, RocketMenuPlugin *menu):ConfirmableAction(back_page),context(context), menu(menu) {}
        virtual void Yes() {
            dnQuitToTitle();
            menu->ShowDocument(context, "menu-main", false);
        }
    };
    Rocket::Core::ElementDocument *page = element->GetOwnerDocument();
    ShowConfirmation(new QuitToTitleAction(page, m_context, m_menu), "yesno", "Quit To Title?");
}

void GUI::QuitGameCommand(Rocket::Core::Element *element) {

    struct QuitGameAction: public ConfirmableAction {
        QuitGameAction(Rocket::Core::ElementDocument *page):ConfirmableAction(page){}
        virtual void Yes() {
            dnQuitGame();
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
    SetActionToConfirm(action);
}

void GUI::PopulateOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element) {
	char buffer[64];
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
}

void GUI::DidOpenMenuPage(Rocket::Core::ElementDocument *menu_page) {
	Rocket::Core::String page_id(menu_page->GetId());
    
    intomenusounds();
    
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
        }
    } else if (page_id == "menu-keys-setup") {
        InitKeysSetupPage(menu_page);
    } else if (page_id == "menu-mouse-setup") {
        InitMouseSetupPage(menu_page);
    } else if (page_id == "menu-load" || page_id == "menu-save") {
        InitLoadPage(menu_page);
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
                if (i != 0) {
                    menu_item->SetInnerRML(saveh.name);
                }
                menu_item->SetClass("empty", false);
                menu_item->RemoveAttribute("noanim");
            } else {
                if (i != 0) {
                    menu_item->SetInnerRML("EMPTY");
                }
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
}

void GUI::InitVideoOptionsPage(Rocket::Core::ElementDocument *page) {
    VideoMode vm;
    dnGetCurrentVideoMode(&vm);
    SetChosenMode(&vm);
    UpdateApplyStatus();
    m_menu->ActivateOption(m_menu->GetMenuItem(page, "texture-filter"), gltexfiltermode < 3 ? "retro" : "smooth", false);
    m_menu->SetRangeValue(m_menu->GetMenuItem(page, "gamma"), (float)dnGetBrightness(), false);
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
}

void GUI::InitKeysSetupPage(Rocket::Core::ElementDocument *page) {
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

//                int key0 = KeyboardKeys[func_num][0];
//                int key1 = KeyboardKeys[func_num][1];
//                const char *key0_name = key0 == -1 ? "None" : dnGetKeyName(key0);
//                const char *key1_name = key1 == -1 ? "None" : dnGetKeyName(key1);
//                m_menu->SetKeyChooserValue(menu_item, 0, KeyDisplayName(key0_name), key0_name);
//                m_menu->SetKeyChooserValue(menu_item, 1, KeyDisplayName(key1_name), key1_name);
            }
        }
}

void GUI::DidCloseMenuPage(Rocket::Core::ElementDocument *menu_page) {
    const Rocket::Core::String& page_id = menu_page->GetId();
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
    }
}

void GUI::ApplyVideoMode(Rocket::Core::ElementDocument *menu_page) {
    
    struct ApplyVideoModeAction: public ConfirmableAction {
        VideoMode new_mode;
        VideoMode backup_mode;
        ApplyVideoModeAction(Rocket::Core::ElementDocument *back_page):ConfirmableAction(back_page){}
        virtual void Yes() {
            printf("Apply Video Mode\n");
            SaveVideoMode(&new_mode);
        }
        virtual void No() {
            printf("Restore Video Mode\n");
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
	} else if (item_id == "auto-aiming") {
		AutoAim = ( value_id == "autoaim-on" ? 1 : 0 );
		ps[myconnectindex].auto_aim = AutoAim;
	} else if (item_id == "run-key-style") {
		ud.runkey_mode = ( value_id == "runkey-classic" ? 1 : 0 );
	} else if (item_id == "weapon-switch") {
		int v = 3;
		sscanf(value_id.CString(), "autoswitch-%d", &v);
		ud.weaponswitch = v;
		ps[myconnectindex].weaponswitch = ud.weaponswitch;
	} else if (item_id == "statusbar") {
		int v = 2;
		sscanf(value_id.CString(), "statusbar-%d", &v);
		ud.screen_size = v*4;
		ud.statusbarscale = 100;
		vscrn();
	}
}

void GUI::DidChangeRangeValue(Rocket::Core::Element *menu_item, float new_value) {
	Rocket::Core::String item_id(menu_item->GetId());
	if (item_id == "gamma") {
		int brightness = clamp((int)new_value, 0, 63);
		dnSetBrightness(brightness);
	} else if (item_id == "sound-volume") {
		dnSetSoundVolume(clamp((int)new_value, 0, 255));
	} else if (item_id == "music-volume") {
		dnSetMusicVolume(clamp((int)new_value, 0, 255));
	}
}

void GUI::DidRequestKey(Rocket::Core::Element *menu_item, int slot) {
    m_waiting_for_key = true;
    m_pressed_key = SDLK_UNKNOWN;
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
    if (sscanf(menu_item->GetId().CString(), "slot%d", &slot) == 1) {
        Rocket::Core::ElementDocument *doc = menu_item->GetOwnerDocument();
        Rocket::Core::Element *thumbnail = doc->GetElementById("thumbnail");
        Rocket::Core::Element *skill = doc->GetElementById("skill");
        Rocket::Core::Element *episode = doc->GetElementById("episode");
        Rocket::Core::Element *level = doc->GetElementById("level");
        
        if (slot >= 0  && slot < 10) {
            char thumb_rml[50];
            char level_rml[50];
            struct savehead saveh;
            if (loadpheader(slot, &saveh) == 0) {
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
    }
}

void GUI::DidClearKeyChooserValue(Rocket::Core::Element *menu_item, int slot) {
    AssignFunctionKey(menu_item->GetId(), "", slot);
}
