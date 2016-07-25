//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef ROCKET_MENU_PLUGIN_H
#define ROCKET_MENU_PLUGIN_H

#include <Rocket/Core.h>
#include <Rocket/Core/Plugin.h>
#include "RocketAnimationPlugin.h"

struct OptionsData;

struct RocketMenuDelegate {
	virtual bool DoCommand(Rocket::Core::Element *element, const Rocket::Core::String& command) = 0;
	virtual void PopulateOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element) = 0;

	virtual void DidOpenMenuPage(Rocket::Core::ElementDocument *menu_page) = 0;
	virtual void DidCloseMenuPage(Rocket::Core::ElementDocument *menu_page) = 0;

    virtual bool WillChangeOptionValue(Rocket::Core::Element *menu_item, int direction) { return true; }
	virtual void DidChangeOptionValue(Rocket::Core::Element *menu_item, Rocket::Core::Element *new_value) = 0;

	virtual void DidChangeRangeValue(Rocket::Core::Element *menu_item, float new_value) = 0;
    virtual void DidClearKeyChooserValue(Rocket::Core::Element *menu_item, int slot) = 0;
    
    virtual void DidClearItem(Rocket::Core::Element *menu_item) = 0;
    
    virtual void DidRefreshItem(Rocket::Core::Element *menu_item) = 0;
    
    virtual void DidResetItem(Rocket::Core::Element *menu_item) = 0;

    virtual void DidRequestKey(Rocket::Core::Element *menu_item, int slot) = 0;
    
    virtual void DidActivateItem(Rocket::Core::Element *menu_item) = 0;
    
};

enum ItemAction {
	ItemActionEnter,
	ItemActionLeft,
	ItemActionRight,
    ItemActionClear,
    ItemActionRefresh,
    ItemActionReset,
};

class RocketMenuPlugin: public Rocket::Core::Plugin, public Rocket::Core::EventListener {
public:
	RocketMenuPlugin();
	virtual ~RocketMenuPlugin();

	// Called when Rocket is initialised.
	virtual void OnInitialise();
	
	// Called when Rocket shuts down.
	virtual void OnShutdown();

	// Called when a document load request occurs, before the document's file is opened.
	virtual void OnDocumentOpen(Rocket::Core::Context* context, const Rocket::Core::String& document_path);

	// Called when a document is successfully loaded from file or instanced, initialised and added to its context. This is called before the document's 'load' event.
	virtual void OnDocumentLoad(Rocket::Core::ElementDocument* document);

	// Called when a document is unloaded from its context. This is called after the document's 'unload' event.
	virtual void OnDocumentUnload(Rocket::Core::ElementDocument* document);

	// Called when a new context is created.
	virtual void OnContextCreate(Rocket::Core::Context* context);

	// Called when a context is destroyed.
	virtual void OnContextDestroy(Rocket::Core::Context* context);

	// Called when a new element is created.
	virtual void OnElementCreate(Rocket::Core::Element* element);

	// Called when an element is destroyed.
	virtual void OnElementDestroy(Rocket::Core::Element* element);

	virtual void ProcessEvent(Rocket::Core::Event& event); 

	virtual bool GoBack(Rocket::Core::Context *context, bool notify_delegate = true);
	virtual void ShowDocument(Rocket::Core::Context* context, const Rocket::Core::String& id, bool backlink = true);
	
	virtual void ShowModalDocument(Rocket::Core::Context* context, const Rocket::Core::String& id);
	virtual void HideModalDocument(Rocket::Core::Context* context);

	virtual void SetAnimationPlugin(RocketAnimationPlugin *p);

	virtual void HighlightItem(Rocket::Core::Element *e);
	virtual void HighlightItem(Rocket::Core::ElementDocument *document, const Rocket::Core::String& id);
	virtual void HighlightItem(Rocket::Core::Context *context, const Rocket::Core::String& docId, const Rocket::Core::String& elementId);
	virtual void HighlightItem(Rocket::Core::Context *context, const Rocket::Core::String& elementId);

	virtual void HighlightNextItem(Rocket::Core::ElementDocument *document);
	virtual void HighlightNextItem(Rocket::Core::Context *context, const Rocket::Core::String& docId = "");

	virtual void HighlightPreviousItem(Rocket::Core::ElementDocument *document);
	virtual void HighlightPreviousItem(Rocket::Core::Context *context, const Rocket::Core::String& docId = "");

	virtual void DoItemAction(ItemAction action, Rocket::Core::Element *e);
	virtual void DoItemAction(ItemAction action, Rocket::Core::ElementDocument *document, const Rocket::Core::String& id);
	virtual void DoItemAction(ItemAction action, Rocket::Core::Context *context, const Rocket::Core::String& docId, const Rocket::Core::String& elementId);
	virtual void DoItemAction(ItemAction action, Rocket::Core::Context *context, const Rocket::Core::String& docId = "");

	virtual Rocket::Core::Element* GetHighlightedItem(Rocket::Core::ElementDocument *doc);
	virtual Rocket::Core::Element* GetHighlightedItem(Rocket::Core::Context *context, const Rocket::Core::String& docId);

	virtual void SetDelegate(RocketMenuDelegate *delegate);
	virtual RocketMenuDelegate* GetDelegate();

	virtual void ActivateNextOption(Rocket::Core::Element *menu_item);
	virtual void ActivatePreviousOption(Rocket::Core::Element *menu_item);
	virtual void ActivateOption(Rocket::Core::Element *menu_item, Rocket::Core::Element *option, bool notify_delegate = true);
	virtual void ActivateOption(Rocket::Core::Element *menu_item, const Rocket::Core::String& option_id, bool notify_delegate = true);

	virtual Rocket::Core::ElementDocument* GetCurrentPage(Rocket::Core::Context *context);
	virtual Rocket::Core::Element* GetMenuItem(Rocket::Core::ElementDocument *doc, const Rocket::Core::String& id);
	virtual Rocket::Core::Element* GetMenuItem(Rocket::Core::ElementDocument *doc, int index);
	virtual int GetNumMenuItems(Rocket::Core::ElementDocument *doc);
	virtual Rocket::Core::Element* GetActiveOption(Rocket::Core::Element *menu_item);
	virtual Rocket::Core::Element* GetOptionById(Rocket::Core::Element *menu_item, const Rocket::Core::String& option_id);

	virtual void SetRangeValue(Rocket::Core::Element *menu_item, float value, bool notify_delegate = true);
	virtual float GetRangeValue(Rocket::Core::Element *menu_item);

	virtual void IncreaseRangeValue(Rocket::Core::Element *menu_item);
	virtual void DecreaseRangeValue(Rocket::Core::Element *menu_item);

	virtual void SetNextItemValue(Rocket::Core::Element *menu_item);
	virtual void SetPreviousItemValue(Rocket::Core::Element *menu_item);

	virtual void SetNextItemValue(Rocket::Core::Context *context);
	virtual void SetPreviousItemValue(Rocket::Core::Context *context);

    virtual void SetKeyChooserValue(Rocket::Core::Element *menu_item, int slot, const Rocket::Core::String& text, const Rocket::Core::String& id);
    virtual const Rocket::Core::String GetKeyChooserValue(Rocket::Core::Element *menu_item, int slot);

    virtual void ClearMenuItem(Rocket::Core::Element *menu_item);
    virtual void SetupMenuItem(Rocket::Core::Element *element);
    virtual void RefreshMenuItem(Rocket::Core::Element *menu_item);
    virtual void ResetMenuItem(Rocket::Core::Element *menu_item);

    
    virtual void UpdateFocus(Rocket::Core::Context *context);
    
    virtual Rocket::Core::Element* GetFocusTarget(Rocket::Core::Element *menu_item);
    
private:
	RocketAnimationPlugin *m_animation;
	RocketMenuDelegate *m_delegate;

	virtual void SetupOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element);
	virtual void SetupRange(Rocket::Core::Element *menu_item, Rocket::Core::Element *range_element);
	virtual void SetupKeyChooser(Rocket::Core::Element *element);
	virtual void ActivateOption(OptionsData *data, Rocket::Core::ElementList::iterator next, bool notify_delegate = true);
	virtual Rocket::Core::Element* FindNextItem(Rocket::Core::Element *menu_item);
	virtual Rocket::Core::Element* FindPreviousItem(Rocket::Core::Element *menu_item);

    virtual void RequestKeyForKeyChooser(Rocket::Core::Element *menu_item);    
};

#endif /* ROCKET_MENU_PLUGIN_H */
