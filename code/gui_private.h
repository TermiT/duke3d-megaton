//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef GUI_PRIVATE_H
#define GUI_PRIVATE_H

#include <SDL.h>
#include <Rocket/Core.h>
#include <Rocket/Core/SystemInterface.h>
#include <Rocket/Core/RenderInterface.h>
#include <Rocket/Debugger.h>
#include "RocketMenuPlugin.h"
#include "RocketAnimationPlugin.h"
#include "ShellRenderInterfaceOpenGL.h"
#include "dnAPI.h"
#include "csteam.h"

class MessageBoxManager;
class LobbyInfoRequestManager;
class WorkshopRequestManager;

struct ConfirmableAction;

class GUI: public RocketMenuDelegate {
private:
	int m_width, m_height;
	Rocket::Core::SystemInterface *m_systemInterface;
	ShellRenderInterfaceOpenGL *m_renderInterface;
	Rocket::Core::Context *m_context;
	RocketMenuPlugin *m_menu;
	RocketAnimationPlugin *m_animation;
	
	MessageBoxManager *m_messageBoxManager;
	LobbyInfoRequestManager *m_lobbyInfoRequestManager;
	WorkshopRequestManager *m_workshopRequestManager;

	bool m_enabled_for_current_frame;
	bool m_enabled;
	
	float m_menu_scale, m_menu_offset_x, m_menu_offset_y;
	int m_mouse_x, m_mouse_y;

    bool m_waiting_for_key;
    dnKey m_pressed_key;
    Rocket::Core::Element *m_waiting_menu_item;
    int m_waiting_slot;
    
    bool m_need_apply_video_mode, m_need_apply_vsync;
    VideoMode m_backup_video_mode, m_new_video_mode;
    
    bool m_show_press_enter;
    bool m_draw_strips;
    
    double m_last_poll;
    
    unsigned int m_pingval;
    double m_pingtime;
    steam_id_t m_ping_subj;
    double m_latency;
    int m_ping_attempt;
    int m_num_failed_attempts;
    SDL_TimerID m_ping_timer_id;

    int m_setBrightness;
    double m_lastBrightnessUpdate;

    steam_id_t m_lobby_id, m_invited_lobby;
    bool m_join_on_launch;
    
    ConfirmableAction *m_action_to_confirm;
    
    Rocket::Core::String menu_to_open;

	void SetupAnimation();
	void LoadDocuments();
	void LoadDocument(const Rocket::Core::String& path);
    void GetAddonDocumentPath(int addon, const Rocket::Core::String& path, char * addonPath);

	void Reload();

	void ReadChosenMode(VideoMode *mode);
	void SetChosenMode(const VideoMode *mode);
	void UpdateApplyStatus();
	void ReadChosenSkillAndEpisode(int *skill, int *episode);
    void Poll();
    
    void SetActionToConfirm(ConfirmableAction *action);
    
    void CreateLobby();
    void FillLobbyInfo(lobby_info_t *lobby_info);
    void JoinLobby(steam_id_t lobby_id);
    void UpdateLobbyList();
    void StartLobby();
    bool VerifyLobby(lobby_info_t *lobby_info);
    
    void UpdateLobbyPage(lobby_info_t *lobby_info);
    
    bool GetOptionNumericValue(Rocket::Core::ElementDocument *menu_page, const Rocket::Core::String& option_id, const Rocket::Core::String& prefix, int *value);
    
    void ShowErrorMessage(const char *page_id, const char *message);
    
    void NewNetworkGame(lobby_info_t *lobby_info);
	   
	void ProcessKeyDown( dnKey key );
public:
	GUI(int width, int height);
	~GUI();
	void PreModeChange();
	void PostModeChange(int width, int height);
	void Render();
	void Enable(bool v);
	bool IsEnabled() { return m_enabled; }
	bool InjectEvent(SDL_Event *ev);
	void TimePulse();
	void UpdateMenuTransform();
	void EnableForCurrentFrame();
	Rocket::Core::Vector2i TranslateMouse(int x, int y);

	virtual bool DoCommand(Rocket::Core::Element *element, const Rocket::Core::String& command);
	virtual void PopulateOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element);
	void DidOpenMenuPage(Rocket::Core::ElementDocument *menu_page);
	void DidCloseMenuPage(Rocket::Core::ElementDocument *menu_page);
    bool WillChangeOptionValue(Rocket::Core::Element *menu_item, int direction);
	void DidChangeOptionValue(Rocket::Core::Element *menu_item, Rocket::Core::Element *new_value);
	virtual void DidChangeRangeValue(Rocket::Core::Element *menu_item, float new_value);
    virtual void DidRequestKey(Rocket::Core::Element *menu_item, int slot);
    virtual void DidActivateItem(Rocket::Core::Element *menu_item);
    virtual void DidClearKeyChooserValue(Rocket::Core::Element *menu_item, int slot);
    virtual void DidClearItem(Rocket::Core::Element *menu_item);
    virtual void DidRefreshItem(Rocket::Core::Element *menu_item);
    virtual void DidResetItem(Rocket::Core::Element *menu_item);

    void InitKeysSetupPage(Rocket::Core::ElementDocument *page);

    void InitGameOptionsPage(Rocket::Core::ElementDocument *page);

    void InitSoundOptionsPage(Rocket::Core::ElementDocument *page);

    void InitVideoOptionsPage(Rocket::Core::ElementDocument *page);

    void ApplyNewKeysSetup(Rocket::Core::ElementDocument *menu_page);

    void HideKeyPrompt();

    void AssignFunctionKey(Rocket::Core::String const & function_name, Rocket::Core::String const & key_id, int slot);

    void InitMouseSetupPage(Rocket::Core::ElementDocument *menu_page);
    
    void InitGamepadSetupPage(Rocket::Core::ElementDocument *menu_page);

    void InitLoadPage(Rocket::Core::ElementDocument *menu_page);
    
    void InitUserMapsPage(const char * page_id);
    
    void InitMpMapsPage(Rocket::Core::ElementDocument *menu_page);
    
    void InitLobbyList(Rocket::Core::ElementDocument *menu_page);
    
    void InitLobbyFilterPage(Rocket::Core::ElementDocument *page);

    void ShowConfirmation(ConfirmableAction *action, const Rocket::Core::String& document, const Rocket::Core::String& text, const Rocket::Core::String& default_option="yes");

    void LoadGameCommand(Rocket::Core::Element *element);

    void SaveGameCommand(Rocket::Core::Element *element);

    void QuitGameCommand(Rocket::Core::Element *element);

    void QuitToTitleCommand(Rocket::Core::Element *element);
    void LeaveLobbyCommand(Rocket::Core::Element *element);

    void ApplyVideoMode(Rocket::Core::ElementDocument *menu_page);

    void ResetMouse();
    
    void ShowMenuByID(const char * menu_id);
    void ShowSaveMenu();
    void ShowLoadMenu();
    void ShowHelpMenu();
    void ShowSoundMenu();
    void ShowVideoSettingsMenu();
    void ShowGameOptionsMenu();
    void ShowQuitConfirmation();
    
    void OnLobbyCreated(lobby_info_t *lobby_info);
    void OnLobbyListUpdated();
    void OnLobbyEnter(lobby_info_t *lobby_info);
    void OnLobbyMembersChanged(lobby_info_t *lobby_info);
    void OnLobbyMessage(lobby_message_t *message);
    void OnLobbyStarted(lobby_info_t *lobby_info);
    void OnLobbyDataUpdate(lobby_info_t *lobby_info, int success);
    void OnLobbyJoinInvite(steam_id_t lobby_id);
	void OnWorkshopSubscribe(workshop_item_t *item, int status);
    void OnBigPictureTextUpdate(const char* data, int status);
    void CancelLobbyJoinIntention();
    
};

#endif /* GUI_PRIVATE_H */
