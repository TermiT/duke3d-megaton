//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "RocketMenuPlugin.h"
#include <Rocket/Core.h>
#include <Rocket/Core/Element.h>
#include "PluginUtils.h"
#include "helpers.h"
#include "math.h"


static
void SetActiveKeySlot(Rocket::Core::Element *menu_item, int active_key);

/*** Context Data ***/

struct ContextData {
	Rocket::Core::ElementDocument *current_document;
	Rocket::Core::ElementDocument *saved_current_document;

	ContextData():current_document(0),saved_current_document(0) {}
	virtual ~ContextData(){};
};

static void
SetContextData(Rocket::Core::Context *context, ContextData *user_data) {
	SetElementUserData(context->GetRootElement(), (void*)user_data, "__menu_context_data");
}

static ContextData*
GetContextData(Rocket::Core::Context *context) {
	return (ContextData*)GetElementUserData(context->GetRootElement(), "__menu_context_data");
}

/*** Document Data ***/

struct DocumentData {
	Rocket::Core::Element *menu;
	Rocket::Core::Element *active_item;
	Rocket::Core::Element *cursor_left, *cursor_right;
	int active_key;

	DocumentData():active_item(0),menu(0),cursor_left(0),cursor_right(0),active_key(0) {}
	virtual ~DocumentData() {};
};

static void
SetDocumentData(Rocket::Core::ElementDocument *document, DocumentData *doc_data) {
	SetElementUserData(document, (void*)doc_data, "__menu_doc_data");
}

static DocumentData*
GetDocumentData(Rocket::Core::ElementDocument *document) {
	return (DocumentData*)GetElementUserData(document, "__menu_doc_data");
}

/*** Options Data ***/

struct OptionsData {
	Rocket::Core::Element *menu_item;
	Rocket::Core::Element *hdr, *ftr;
	Rocket::Core::ElementList options;
	Rocket::Core::ElementList::iterator current;

	OptionsData():menu_item(NULL),hdr(NULL),ftr(NULL){}
	virtual ~OptionsData() {};
};

static void
SetOptionsData(Rocket::Core::Element *element, OptionsData *data) {
	SetElementUserData(element, (void*)data, "__menu_options_data");
}

static OptionsData*
GetOptionsData(Rocket::Core::Element *element) {
	return (OptionsData*)GetElementUserData(element, "__menu_options_data");
}

/*** Range Data ***/

struct RangeData {
	Rocket::Core::Element *menu_item;
	Rocket::Core::Element *hdr, *ftr;
    Rocket::Core::Element *range;
//	Rocket::Core::Element *rail, *grip;
//	float rail_x, rail_y, rail_length;
	float min, max, value, step;

	RangeData():menu_item(NULL),hdr(NULL),ftr(NULL),range(NULL),
//        rail(NULL),grip(NULL),
		min(0.0f),max(1.0f),value(0.0f),step(0.1f)
	{}
	virtual ~RangeData() {};
};

static void
SetRangeData(Rocket::Core::Element *element, RangeData *data) {
	SetElementUserData(element, (void*)data, "__menu_range_data");
}

static RangeData*
GetRangeData(Rocket::Core::Element *element) {
	return (RangeData*)GetElementUserData(element, "__menu_range_data");
}

/*** Key Data ***/

struct KeyData {
	Rocket::Core::Element *menu_item;
	Rocket::Core::Element *hdr1, *ftr1;
	Rocket::Core::Element *hdr2, *ftr2;
	Rocket::Core::Element *opt1, *opt2;

	KeyData():menu_item(NULL),hdr1(NULL),ftr1(NULL),hdr2(NULL),ftr2(NULL),opt1(NULL),opt2(NULL){}
	virtual ~KeyData(){}
};

static void
SetKeyData(Rocket::Core::Element *element, KeyData *data) {
	SetElementUserData(element, (void*)data, "__menu_key_data");
}

static KeyData*
GetKeyData(Rocket::Core::Element *element) {
	return (KeyData*)GetElementUserData(element, "__menu_key_data");
}

/************/

RocketMenuPlugin::RocketMenuPlugin():m_animation(0),m_delegate(0) {
}

RocketMenuPlugin::~RocketMenuPlugin() {
}

void RocketMenuPlugin::OnInitialise() {
}

void RocketMenuPlugin::OnShutdown() {
}

void RocketMenuPlugin::OnDocumentOpen(Rocket::Core::Context* context, const Rocket::Core::String& document_path) {
}

void RocketMenuPlugin::SetupOptions(Rocket::Core::Element *menu_item, Rocket::Core::Element *options_element) {
	OptionsData *data = new OptionsData();
	SetOptionsData(menu_item, data);
	data->menu_item = menu_item;

	options_element->GetElementsByTagName(data->options, "option");
	if (data->options.size() == 0) {
		if (m_delegate != NULL) {
			m_delegate->PopulateOptions(menu_item, options_element);
			options_element->GetElementsByTagName(data->options, "option");
		}
	}

	data->hdr = Rocket::Core::Factory::InstanceElement(options_element, "hdr", "hdr", Rocket::Core::XMLAttributes());
	data->hdr->AddEventListener("click", this);
	data->hdr->SetInnerRML("&lt;");
	ShowElement(data->hdr, false);
	options_element->InsertBefore(data->hdr, options_element->GetFirstChild());
	data->hdr->RemoveReference();

	data->ftr = Rocket::Core::Factory::InstanceElement(options_element, "ftr", "ftr", Rocket::Core::XMLAttributes());
	data->ftr->AddEventListener("click", this);
	data->ftr->SetInnerRML("&gt;");
	ShowElement(data->ftr, false);
	options_element->AppendChild(data->ftr);
	data->ftr->RemoveReference();


	for (Rocket::Core::ElementList::iterator i = data->options.begin(); i != data->options.end(); i++) {
		Rocket::Core::Element *element = *i;
		ShowElement(element, false);
	}
	data->current = data->options.begin();
	ShowElement(*data->current, true);
}

void RocketMenuPlugin::SetupRange(Rocket::Core::Element *menu_item, Rocket::Core::Element *range_element) {
	RangeData *data = new RangeData();
	SetRangeData(menu_item, data);
	data->menu_item = menu_item;
    
    if (range_element->HasAttribute("min")) {
		data->min = range_element->GetAttribute("min")->Get<float>();
	}
	if (range_element->HasAttribute("max")) {
		data->max = range_element->GetAttribute("max")->Get<float>();
	}
	if (range_element->HasAttribute("value")) {
		data->value = range_element->GetAttribute("value")->Get<float>();
	}
	if (range_element->HasAttribute("step")) {
		data->step = range_element->GetAttribute("step")->Get<float>();
	}

	data->hdr = Rocket::Core::Factory::InstanceElement(range_element, "hdr", "hdr", Rocket::Core::XMLAttributes());
	data->hdr->AddEventListener("click", this);
	data->hdr->SetInnerRML("&lt;");
	ShowElement(data->hdr, false);
	//options_element->InsertBefore(data->hdr, options_element->GetFirstChild());
	range_element->AppendChild(data->hdr);
	data->hdr->RemoveReference();


    Rocket::Core::XMLAttributes input_attrs;
    input_attrs.Set("type", "range");
    input_attrs.Set("min", 0.0f);
    input_attrs.Set("max", 1.0f);
    input_attrs.Set("step", 0.001f);
    input_attrs.Set("value", (data->value - data->min)/(data->max - data->min));
    data->range = Rocket::Core::Factory::InstanceElement(range_element, "input", "input", input_attrs);
    data->range->AddEventListener("change", this);
    data->range->SetProperty("margin-top", "15px");
    range_element->AppendChild(data->range);
    data->range->RemoveReference();
	data->ftr = Rocket::Core::Factory::InstanceElement(range_element, "ftr", "ftr", Rocket::Core::XMLAttributes());
	data->ftr->AddEventListener("click", this);
	data->ftr->SetInnerRML("&gt;");
	ShowElement(data->ftr, false);
	range_element->AppendChild(data->ftr);
	data->ftr->RemoveReference();

	SetRangeValue(menu_item, data->value, false);
}

void RocketMenuPlugin::SetupKeyChooser(Rocket::Core::Element *element) {
	Rocket::Core::String caption;

	element->GetInnerRML(caption);
	element->SetInnerRML("<caption/><keys><key1><hdr>&lt;</hdr><keyval/><ftr>&gt;</ftr></key1><key2><hdr>&lt;</hdr><keyval/><ftr>&gt;</ftr></key2></keys>");

	Rocket::Core::Element *caption_element = element->GetChild(0);
	caption_element->SetInnerRML(caption);

	Rocket::Core::Element *keys_element = element->GetChild(1);
	Rocket::Core::Element *key1_element = keys_element->GetChild(0);
	Rocket::Core::Element *key2_element = keys_element->GetChild(1);

    key1_element->AddEventListener("mousemove", this, false);
    key2_element->AddEventListener("mousemove", this, false);

	KeyData *data = new KeyData();
	data->menu_item = element;
	data->hdr1 = key1_element->GetChild(0);
	data->opt1 = key1_element->GetChild(1);
	data->ftr1 = key1_element->GetChild(2);

	data->hdr2 = key2_element->GetChild(0);
	data->opt2 = key2_element->GetChild(1);
	data->ftr2 = key2_element->GetChild(2);

	data->opt1->SetInnerRML("None");

	data->opt2->SetInnerRML("None");

	ShowElement(data->hdr1, false);
	ShowElement(data->ftr1, false);
	ShowElement(data->hdr2, false);
	ShowElement(data->ftr2, false);

	SetKeyData(element, data);
}

void RocketMenuPlugin::SetupMenuItem(Rocket::Core::Element *element) {
	element->AddEventListener("click", this);
	element->AddEventListener("mousemove", this);
	element->AddEventListener("mouseout", this);
	if (element->HasAttribute("options") && element->GetNumChildren() > 0) {
		Rocket::Core::ElementList options_list;
		element->GetElementsByTagName(options_list, "options");
		if (options_list.size() > 0) {
			SetupOptions(element, options_list[0]);
		}
	}
	if (element->HasAttribute("range") && element->GetNumChildren() > 0) {
		Rocket::Core::ElementList elist;
		element->GetElementsByTagName(elist, "range");
		if (elist.size() > 0) {
			SetupRange(element, elist[0]);
		}
	}
	if (element->HasAttribute("key-chooser")) {
		SetupKeyChooser(element);
	}
}

void RocketMenuPlugin::OnDocumentLoad(Rocket::Core::ElementDocument* document) {
	DocumentData *doc_data = new DocumentData();

	doc_data->cursor_left = document->GetElementById("cursor-left");
	doc_data->cursor_right = document->GetElementById("cursor-right");
	doc_data->menu = document->GetElementById("menu");

	if (doc_data->menu == NULL) {
		for (int i = 0, n = document->GetNumChildren(); i!=n; i++) {
			Rocket::Core::Element *element = document->GetChild(i);
			if (element->IsClassSet("game-menu")) {
				doc_data->menu = element;
				break;
			}
		}
	}

	if (doc_data->menu != NULL) {
		for (int i = 0, n = doc_data->menu->GetNumChildren(); i < n; i++) {
			Rocket::Core::Element *e = doc_data->menu->GetChild(i);
			SetupMenuItem(e);
		}
		SetDocumentData(document, doc_data);
	} else {
		delete doc_data;
	}
}

// Called when a document is unloaded from its context. This is called after the document's 'unload' event.
void RocketMenuPlugin::OnDocumentUnload(Rocket::Core::ElementDocument* document) {
	delete GetDocumentData(document);
}

// Called when a new context is created.
void RocketMenuPlugin::OnContextCreate(Rocket::Core::Context* context) {
	SetContextData(context, new ContextData());
}

// Called when a context is destroyed.
void RocketMenuPlugin::OnContextDestroy(Rocket::Core::Context* context) {
	ContextData *data = GetContextData(context);
	if (data != NULL) {
		delete data;
		SetContextData(context, NULL);
	}
}

// Called when a new element is created
void RocketMenuPlugin::OnElementCreate(Rocket::Core::Element* element) {
}

// Called when an element is destroyed.
void RocketMenuPlugin::OnElementDestroy(Rocket::Core::Element* element) {
	OptionsData *options_data = GetOptionsData(element);
	if (options_data != NULL) {
		delete options_data;
		SetOptionsData(element, NULL);
	}
	RangeData *range_data = GetRangeData(element);
	if (range_data != NULL) {
		delete range_data;
		SetRangeData(element, NULL);
	}
	KeyData *key_data = GetKeyData(element);
	if (key_data != NULL) {
		delete key_data;
		SetKeyData(element, NULL);
	}
}

class ColorAnimation: public BasicAnimationActuator {
private:
	rgb p, q;
	//Rocket::Core::String initial_color;
	//Rocket::Core::Colourb initial_color;
	void ApplyColorsForPosition(Rocket::Core::Element *e, float position) {
		rgb v = rgb_lerp(p, q, position);
		char color[50];
		int r = (int)(v.r*255);
		int g = (int)(v.g*255);
		int b = (int)(v.b*255);
		sprintf(color, "rgb(%d,%d,%d)", r, g, b);
		e->SetProperty("color", color);;
	}
public:
	ColorAnimation(rgb p, rgb q, float offset):p(p),q(q) {
	}
	virtual void Init(Rocket::Core::Element *element) {
		/*
		initial_color = element->GetProperty<Rocket::Core::Colourb>("color");
		*/
	};
	virtual void Apply(Rocket::Core::Element *e, float position) {
		ApplyColorsForPosition(e, position);
	}
	virtual void Stop(Rocket::Core::Element *e) {};
	virtual void Reset(Rocket::Core::Element *e) {
		e->RemoveProperty("color");
	};
};

void RocketMenuPlugin::ProcessEvent(Rocket::Core::Event& event) {
	Rocket::Core::Element *element = event.GetCurrentElement();

    if (event.GetType() == "click") {
        if (element->GetTagName() == "ftr") {
            SetNextItemValue(element->GetParentNode()->GetParentNode());
            event.StopPropagation();
        } else if (element->GetTagName() == "hdr") {
            SetPreviousItemValue(element->GetParentNode()->GetParentNode());
            event.StopPropagation();
        } else {
            Rocket::Core::Element *focus_target = GetFocusTarget(element);
            if (focus_target == NULL) {
                DoItemAction(ItemActionEnter, element);
            } else {
                element->Focus();
            }
        }
    } else if (event.GetType() == "mousemove") {
        if (element->GetTagName() == "div") {
            HighlightItem(element);
        } else if (element->GetTagName() == "key1") {
            Rocket::Core::Element *menu_item = element->GetParentNode()->GetParentNode();
            SetActiveKeySlot(menu_item, 0);
        } else if (element->GetTagName() == "key2") {
            Rocket::Core::Element *menu_item = element->GetParentNode()->GetParentNode();
            SetActiveKeySlot(menu_item, 1);
        }
    } else if (event.GetType() == "change") {
        if (m_delegate != NULL && element->GetOwnerDocument()->IsVisible()) {
            Rocket::Core::Element *menu_item = element->GetParentNode()->GetParentNode();
            RangeData *data = GetRangeData(menu_item);
            const Rocket::Core::Dictionary  *p = event.GetParameters();
            float v = p->Get("value")->Get<float>();
            float new_value = data->min + v*(data->max - data->min);
            if (fabs(new_value-data->value) > 0.001f) {
                data->value = new_value;
                m_delegate->DidChangeRangeValue(menu_item, data->value);
            }
		}
    }
}

void RocketMenuPlugin::ShowModalDocument(Rocket::Core::Context* context, const Rocket::Core::String& id) {
	ContextData *cd = GetContextData(context);
	
	if (cd->saved_current_document == NULL) {
		
		Rocket::Core::ElementDocument *new_doc = context->GetDocument(id);
		
		if (new_doc != NULL) {
			cd->saved_current_document = cd->current_document;
			cd->current_document = new_doc;
		}
		
		DocumentData *dd = GetDocumentData(cd->current_document);
		if (cd->current_document->HasAttribute("default-item")) {
			Rocket::Core::String def;
			cd->current_document->GetAttribute("default-item")->GetInto(def);
			if (dd->menu != NULL) {
				HighlightItem(cd->current_document->GetElementById(def));
			}
		} else {
			if (dd->active_item == NULL) {
				if (dd->menu != NULL) {
					HighlightItem(dd->menu->GetChild(0));
				}
			}
		}
		if (!cd->current_document->IsVisible()) {
			cd->current_document->Show();
			if (m_delegate) {
				m_delegate->DidOpenMenuPage(cd->current_document);
			}
		}
		
	} else {
		printf("[ROCK] Trying to open modal when modal is active\n" );
	}
}

void RocketMenuPlugin::HideModalDocument(Rocket::Core::Context* context) {
	ContextData *cd = GetContextData(context);
	if (cd->current_document != NULL && cd->saved_current_document != NULL) {
		cd->current_document->Hide();
		if (cd->current_document->HasAttribute("closecmd")) {
			Rocket::Core::String closecmd;
			Rocket::Core::ElementDocument *doc = cd->current_document;
			doc->GetAttribute("closecmd")->GetInto(closecmd);
			doc->RemoveAttribute("closecmd");
			m_delegate->DoCommand(doc, closecmd);
			doc->SetAttribute("closecmd", closecmd);
		}
		cd->current_document = cd->saved_current_document;
		cd->saved_current_document = NULL;
	}
}

void RocketMenuPlugin::ShowDocument(Rocket::Core::Context* context, const Rocket::Core::String& id, bool backlink) {
	ContextData *cd = GetContextData(context);
	Rocket::Core::ElementDocument *new_doc = context->GetDocument(id);
	if (cd->saved_current_document != NULL) {
		HideModalDocument(context);
	}
	if (new_doc != NULL) {
		if (new_doc != cd->current_document) {
			if (cd->current_document != NULL) {
                if (cd->current_document->IsVisible()) {
                    cd->current_document->Hide();
                    if (m_delegate) {
                        m_delegate->DidCloseMenuPage(cd->current_document);
                    }
                }
				if (backlink) {
					new_doc->SetAttribute("parent", cd->current_document->GetId());
				}
			}
			cd->current_document = new_doc;
			DocumentData *dd = GetDocumentData(cd->current_document);
            if (cd->current_document->HasAttribute("default-item")) {
                Rocket::Core::String def;
                cd->current_document->GetAttribute("default-item")->GetInto(def);
                if (dd->menu != NULL) {
                    HighlightItem(cd->current_document->GetElementById(def));
                }
            } else {
                if (dd->active_item == NULL) {
                    if (dd->menu != NULL) {
                        HighlightItem(dd->menu->GetChild(0));
                    }
                }
            }
            if (!cd->current_document->IsVisible()) {
                cd->current_document->Show();
                if (m_delegate) {
                    m_delegate->DidOpenMenuPage(cd->current_document);
                }
            }
		}
	} else {
		printf("[ROCK] unable to show document with id: %s\n", id.CString());
	}
}

bool RocketMenuPlugin::GoBack(Rocket::Core::Context *context, bool notify_delegate) {
	ContextData *cd = GetContextData(context);
	if (cd->current_document != NULL) {
		if (cd->saved_current_document == NULL) {
			Rocket::Core::Variant *attr = cd->current_document->GetAttribute("parent");
			if (attr != NULL) {
				Rocket::Core::String menu_id = attr->Get<Rocket::Core::String>();
				if (cd->current_document->HasAttribute("closecmd") && notify_delegate) {
					Rocket::Core::String closecmd;
					Rocket::Core::ElementDocument *doc = cd->current_document;
					doc->GetAttribute("closecmd")->GetInto(closecmd);
					doc->RemoveAttribute("closecmd");
					if (m_delegate->DoCommand(doc, closecmd)) {
						ShowDocument(context, menu_id, false);
					}
					doc->SetAttribute("closecmd", closecmd);
				} else {
					ShowDocument(context, menu_id, false);
				}
				return true;
			}
		} else {
			HideModalDocument(context);
		}
	}
	return false;
}

void RocketMenuPlugin::SetAnimationPlugin(RocketAnimationPlugin *p) {
	m_animation = p;
}

static
void AttachCursor(Rocket::Core::Element *menu_item, Rocket::Core::Element *cursor_left, Rocket::Core::Element *cursor_right) {
	float item_width = menu_item->GetOffsetWidth();
	float item_height = menu_item->GetOffsetHeight();
    float item_x = menu_item->GetAbsoluteLeft();
    float item_y = menu_item->GetAbsoluteTop();
	if (cursor_left != NULL) {
        float cursor_height = cursor_left->GetOffsetHeight();

        cursor_left->SetProperty("left", Rocket::Core::Property(item_x-cursor_left->GetOffsetWidth(), Rocket::Core::Property::PX));
        cursor_left->SetProperty("top", Rocket::Core::Property(item_y + (item_height-cursor_height)/2, Rocket::Core::Property::PX));

	}
	if (cursor_right != NULL) {
        float cursor_height = cursor_right->GetOffsetHeight();

        cursor_right->SetProperty("left", Rocket::Core::Property(item_x+item_width, Rocket::Core::Property::PX));
        cursor_right->SetProperty("top", Rocket::Core::Property(item_y + (item_height-cursor_height)/2, Rocket::Core::Property::PX));
	}
}

static 
void DoScroll(Rocket::Core::Element *old, Rocket::Core::Element *neww, Rocket::Core::Element *scroll) {
	//float scroll_height = scroll->GetScrollHeight();
	float new_top = neww->GetOffsetTop();
	float new_height = neww->GetOffsetHeight();
	float window_height = scroll->GetOffsetHeight();
	float position = scroll->GetScrollTop();
	if (new_top < position || (new_top + new_height > position + window_height)) {
		if (old == NULL) {
			position = new_top+new_height/2-window_height/2;
		} else {
			float old_top = old->GetOffsetTop();
			if (old_top < new_top) {
				position = new_top+new_height-window_height;
			} else {
				position = new_top;
			}
		}
		scroll->SetScrollTop(position);
	}
}

static void
UpdateKeyChooser(KeyData *data, int active_key, bool highlight) {
	ShowElement(data->hdr1, highlight && !active_key);
	ShowElement(data->ftr1, highlight && !active_key);
	ShowElement(data->hdr2, highlight && active_key);
	ShowElement(data->ftr2, highlight && active_key);
}

Rocket::Core::Element* RocketMenuPlugin::GetFocusTarget(Rocket::Core::Element *menu_item) {
    Rocket::Core::Element *result = NULL;
    if (menu_item != NULL && menu_item->HasAttribute("focus")) {
        result = menu_item->GetOwnerDocument()->GetElementById(menu_item->GetAttribute("focus")->Get<Rocket::Core::String>());
    }
    return result;
}

void RocketMenuPlugin::HighlightItem(Rocket::Core::Element *e) {
	static ColorAnimation colorAnimation(rgb(255,255,255), rgb(235, 156, 9), 1);
	DocumentData *doc_data = GetDocumentData(e->GetOwnerDocument());
	if (doc_data != NULL) {
		if (doc_data->active_item != e && !e->IsClassSet("disabled")) {
			if (doc_data->active_item != NULL) {
				m_animation->CancelAnimation(doc_data->active_item);
				OptionsData *options_data = GetOptionsData(doc_data->active_item);
				if (options_data != NULL) {
					ShowElement(options_data->hdr, false);
					ShowElement(options_data->ftr, false);
				}
				RangeData *range_data = GetRangeData(doc_data->active_item);
				if (range_data != NULL) {
					ShowElement(range_data->hdr, false);
					ShowElement(range_data->ftr, false);
				}
				KeyData *key_data = GetKeyData(doc_data->active_item);
				if (key_data != NULL) {
					UpdateKeyChooser(key_data, doc_data->active_key, false);
				}
			}
			if (e != NULL) {
				if (e->GetParentNode()->HasAttribute("scroll")) {
					DoScroll(doc_data->active_item, e, e->GetParentNode()->GetParentNode());
				}
                if (!e->HasAttribute("noanim")) {
                    m_animation->AnimateElement(e, AnimationTypeBounce, 0.3f, &colorAnimation);
                }
				AttachCursor(e, doc_data->cursor_left, doc_data->cursor_right);
				OptionsData *options_data = GetOptionsData(e);
				if (options_data != NULL) {
					ShowElement(options_data->hdr, true);
					ShowElement(options_data->ftr, true);
				}
				RangeData *range_data = GetRangeData(e);
				if (range_data != NULL) {
					ShowElement(range_data->hdr, true);
					ShowElement(range_data->ftr, true);
				}
				KeyData *key_data = GetKeyData(e);
				if (key_data != NULL) {
					UpdateKeyChooser(key_data, doc_data->active_key, true);
				}
                m_delegate->DidActivateItem(e);
			}
			doc_data->active_item = e;
            if (doc_data->active_item != NULL) {
                UpdateFocus(doc_data->active_item->GetContext());
            }
		}
	}
}

void RocketMenuPlugin::UpdateFocus(Rocket::Core::Context *context) {
    ContextData *cd = GetContextData(context);
    cd->current_document->GetFocusLeafNode()->Blur();
    DocumentData *dd = GetDocumentData(cd->current_document);
    if (dd->active_item != NULL) {
        Rocket::Core::Element *focus_target = GetFocusTarget(dd->active_item);
        if (focus_target != NULL) {
            focus_target->Focus();
        }
    }
}

void RocketMenuPlugin::HighlightItem(Rocket::Core::ElementDocument *document, const Rocket::Core::String& id) {
	Rocket::Core::Element *element = document->GetElementById(id);
	if (element != 0) {
		HighlightItem(element);
	} else {
		printf("[ROCK] RocketMenuPlugin::HighlightItem: unable to find element with id='%s'\n", id.CString());
	}
}

void RocketMenuPlugin::HighlightItem(Rocket::Core::Context *context, const Rocket::Core::String& docId, const Rocket::Core::String& elementId) {
	Rocket::Core::ElementDocument *doc = context->GetDocument(docId);
	if (doc != NULL) {
		HighlightItem(doc, elementId);
	} else {
		printf("[ROCK] RocketMenuPlugin::HighlightItem: unable to find document with id='%s'\n", docId.CString());
	}
}

void RocketMenuPlugin::HighlightItem(Rocket::Core::Context *context, const Rocket::Core::String& elementId) {
	ContextData *cd = GetContextData(context);
	if (cd->current_document != NULL) {
		HighlightItem(cd->current_document, elementId);
	}
}

Rocket::Core::Element* RocketMenuPlugin::FindNextItem(Rocket::Core::Element *menu_item) {
	Rocket::Core::Element *next = menu_item;
    
    do {
        if (next->HasAttribute("next")) {
            Rocket::Core::Context *context = next->GetContext();
            ContextData *cd = GetContextData(context);
            Rocket::Core::ElementDocument *document = cd->current_document;
            next = document->GetElementById(next->GetAttribute("next")->Get<Rocket::Core::String>());
        } else {
            next = next->GetNextSibling();
            if (next == NULL) {
                next = menu_item->GetParentNode()->GetChild(0);
            }
        }
    } while (next->IsClassSet("disabled") && next != menu_item);

	return next;
}

Rocket::Core::Element* RocketMenuPlugin::FindPreviousItem(Rocket::Core::Element *menu_item) {
	Rocket::Core::Element *next = menu_item;
    
    do {
        if (next->HasAttribute("previous")) {
            Rocket::Core::Context *context = next->GetContext();
            ContextData *cd = GetContextData(context);
            Rocket::Core::ElementDocument *document = cd->current_document;
            next = document->GetElementById(next->GetAttribute("previous")->Get<Rocket::Core::String>());
        } else {
            next = next->GetPreviousSibling();
            if (next == NULL) {
                next = menu_item->GetParentNode()->GetChild(menu_item->GetParentNode()->GetNumChildren()-1);
            }
        }
    } while (next->IsClassSet("disabled") && next != menu_item);
    
	return next;
}

void RocketMenuPlugin::HighlightNextItem(Rocket::Core::ElementDocument *document = NULL) {
	DocumentData *doc_data = GetDocumentData(document);
	if (doc_data != NULL) {
		if (doc_data->active_item != NULL) {
			Rocket::Core::Element *e = FindNextItem(doc_data->active_item);
			if (e != NULL) {
				HighlightItem(e);
			}
		}
	} else {
		printf("[ROCK] RocketMenuPlugin::HighlightNextItem: attempt to activate next item on the document with no menu\n");
	}
}

void RocketMenuPlugin::HighlightNextItem(Rocket::Core::Context *context, const Rocket::Core::String& docId) {
	Rocket::Core::ElementDocument *doc = NULL;
	if (docId == "") {
		ContextData *cd = GetContextData(context);
		doc = cd->current_document;
	} else {
		doc = context->GetDocument(docId);
	}
	if (doc != NULL) {
		HighlightNextItem(doc);
	} else {
		printf("[ROCK] RocketMenuPlugin::HighlightNextItem: document not found: %s\n", docId.CString());
	}
}

void RocketMenuPlugin::HighlightPreviousItem(Rocket::Core::ElementDocument *document) {
	DocumentData *doc_data = GetDocumentData(document);
	if (doc_data != NULL) {
		if (doc_data->active_item != NULL) {
			Rocket::Core::Element *e = FindPreviousItem(doc_data->active_item);
			if (e != NULL) {
				HighlightItem(e);
			}
		}
	} else {
		printf("[ROCK] RocketMenuPlugin::HighlightPreviousItem: attempt to activate next item on the document with no menu\n");
	}
}

void RocketMenuPlugin::HighlightPreviousItem(Rocket::Core::Context *context, const Rocket::Core::String& docId) {
	Rocket::Core::ElementDocument *doc = NULL;
	if (docId == "") {
		ContextData *cd = GetContextData(context);
		doc = cd->current_document;
	} else {
		doc = context->GetDocument(docId);
	}
	if (doc != NULL) {
		HighlightPreviousItem(doc);
	} else {
		printf("[ROCK] RocketMenuPlugin::HighlightPreviousItem: document not found: %s\n", docId.CString());
	}
}

void RocketMenuPlugin::DoItemAction(ItemAction action, Rocket::Core::Element *e) {
	if (!e->IsClassSet("disabled")) {
		switch (action) {
			case ItemActionEnter: {
				if (e->HasAttribute("submenu")) {
					bool backlink = !e->HasAttribute("noref");
					Rocket::Core::String submenu;
					e->GetAttribute("submenu")->GetInto(submenu);
					ShowDocument(e->GetContext(), submenu, backlink);
				}
				if (e->HasAttribute("command") && m_delegate != NULL) {
					Rocket::Core::String command;
					e->GetAttribute("command")->GetInto(command);
					m_delegate->DoCommand(e, command);
				}
				if (e->HasAttribute("options")) {
                    if (m_delegate == NULL || m_delegate->WillChangeOptionValue(e, 0)) {
                        ActivateNextOption(e);
                    }
				}
                RequestKeyForKeyChooser(e);
				break;
			}
			case ItemActionLeft:
				if (e->HasAttribute("options")) {
                    if (m_delegate == NULL || m_delegate->WillChangeOptionValue(e, -1)) {
                        ActivatePreviousOption(e);
                    }
				}
				break;
			case ItemActionRight:
				if (e->HasAttribute("options")) {
                    if (m_delegate == NULL || m_delegate->WillChangeOptionValue(e, +1)) {
                        ActivateNextOption(e);
                    }
				}
				break;
            case ItemActionClear: {
                ClearMenuItem(e);
                break;
            }
            case ItemActionRefresh: {
                RefreshMenuItem(e);
                break;
            }
            case ItemActionReset: {
                ResetMenuItem(e);
                break;
            }


		}
	}
}

void RocketMenuPlugin::DoItemAction(ItemAction action, Rocket::Core::ElementDocument *document, const Rocket::Core::String& id) {
	Rocket::Core::Element *element = document->GetElementById(id);
	if (element != NULL) {
		DoItemAction(action, element);
	} else {
		printf("[ROCK] RocketMenuPlugin::DoItemAction: element not found: %s\n", id.CString());
	}
}

void RocketMenuPlugin::DoItemAction(ItemAction action, Rocket::Core::Context *context, const Rocket::Core::String& docId, const Rocket::Core::String& elementId) {
	Rocket::Core::ElementDocument *doc = context->GetDocument(docId);
	if (doc != NULL) {
		DoItemAction(action, doc, elementId);
	} else {
		printf("[ROCK] RocketMenuPlugin::DoItemAction: document not found: %s\n", docId.CString());
	}
}

void RocketMenuPlugin::DoItemAction(ItemAction action, Rocket::Core::Context *context, const Rocket::Core::String& id) {
	ContextData *cd = GetContextData(context);
	if (cd->current_document != NULL) {
		DocumentData *dd = GetDocumentData(cd->current_document);
		Rocket::Core::Element *e = NULL;
		if (id != "") {
			e = cd->current_document->GetElementById(id);
		} else if (dd != NULL) {
			e = dd->active_item;
		}
		if (e != NULL) {
			DoItemAction(action, e);
		}
	}
}

Rocket::Core::Element* RocketMenuPlugin::GetHighlightedItem(Rocket::Core::ElementDocument *doc) {
	DocumentData *dd = GetDocumentData(doc);
	return dd ? dd->active_item : NULL;
}

Rocket::Core::Element* RocketMenuPlugin::GetHighlightedItem(Rocket::Core::Context *context, const Rocket::Core::String& docId) {
	Rocket::Core::ElementDocument *doc = context->GetDocument(docId);
	return doc ? GetHighlightedItem(doc) : NULL;
}

void RocketMenuPlugin::SetDelegate(RocketMenuDelegate *delegate) {
	m_delegate = delegate;
}

RocketMenuDelegate* RocketMenuPlugin::GetDelegate() {
	return m_delegate;
}

void RocketMenuPlugin::ActivateNextOption(Rocket::Core::Element *menu_item) {
	OptionsData *data = GetOptionsData(menu_item);
	if (data != NULL) {
		Rocket::Core::ElementList::iterator next = data->current+1;
		if (next == data->options.end()) {
			next = data->options.begin();
		}
		ActivateOption(data, next);
	}
}

void RocketMenuPlugin::ActivatePreviousOption(Rocket::Core::Element *menu_item) {
	OptionsData *data = GetOptionsData(menu_item);
	if (data != NULL) {
		Rocket::Core::ElementList::iterator next = data->current;
		if (next == data->options.begin()) {
			next = data->options.end()-1;
		} else {
			next--;
		}
		ActivateOption(data, next);
	}
}

void RocketMenuPlugin::ActivateOption(Rocket::Core::Element *menu_item, Rocket::Core::Element *option, bool notify_delegate) {
	OptionsData *data = GetOptionsData(menu_item);
	if (data != NULL) {
		Rocket::Core::ElementList::iterator next = data->options.end();
		for (Rocket::Core::ElementList::iterator i = data->options.begin(); i != data->options.end(); i++) {
			if (*i == option) {
				next = i;
				break;
			}
		}
		if (next != data->options.end()) {
			ActivateOption(data, next, notify_delegate);
		}
	}
}

void RocketMenuPlugin::ActivateOption(Rocket::Core::Element *menu_item, const Rocket::Core::String& option_id, bool notify_delegate) {
	OptionsData *data = GetOptionsData(menu_item);
	if (data != NULL) {
		Rocket::Core::ElementList::iterator next = data->options.end();
		for (Rocket::Core::ElementList::iterator i = data->options.begin(); i != data->options.end(); i++) {
			if ((*i)->GetId() == option_id) {
				next = i;
				break;
			}
		}
		if (next != data->options.end()) {
			ActivateOption(data, next, notify_delegate);
		}
	}
}

void RocketMenuPlugin::ActivateOption(OptionsData *data, Rocket::Core::ElementList::iterator next, bool notify_delegate) {
	if (next != data->current) {
		ShowElement(*data->current, false);
		ShowElement(*next, true);
		data->current = next;
		if (m_delegate != NULL && notify_delegate) {
			m_delegate->DidChangeOptionValue(data->menu_item, *(data->current));
		}
	}
}

void RocketMenuPlugin::SetNextItemValue(Rocket::Core::Context *context) {
	ContextData *cd = GetContextData(context);
	if (cd != NULL) {
		Rocket::Core::ElementDocument *doc = cd->current_document;
		if (doc != NULL) {
			DocumentData *dd = GetDocumentData(doc);
			if (dd != NULL) {
				SetNextItemValue(dd->active_item);
			}
		}
	}
}

void RocketMenuPlugin::SetPreviousItemValue(Rocket::Core::Context *context) {
	ContextData *cd = GetContextData(context);
	if (cd != NULL) {
		Rocket::Core::ElementDocument *doc = cd->current_document;
		if (doc != NULL) {
			DocumentData *dd = GetDocumentData(doc);
			if (dd != NULL) {
				SetPreviousItemValue(dd->active_item);
			}
		}
	}
}

Rocket::Core::ElementDocument* RocketMenuPlugin::GetCurrentPage(Rocket::Core::Context *context) {
	ContextData *cd = GetContextData(context);
	if (cd != NULL) {
		return cd->current_document;
	}
	return NULL;
}

Rocket::Core::Element* RocketMenuPlugin::GetMenuItem(Rocket::Core::ElementDocument *doc, const Rocket::Core::String& id) {
	DocumentData *dd = GetDocumentData(doc);
	if (dd != NULL) {
		return dd->menu->GetElementById(id);
	}
	return NULL;
}

Rocket::Core::Element* RocketMenuPlugin::GetMenuItem(Rocket::Core::ElementDocument *doc, int index) {
	DocumentData *dd = GetDocumentData(doc);
	if (dd != NULL) {
		return dd->menu->GetChild(index);
	}
	return NULL;
}

int RocketMenuPlugin::GetNumMenuItems(Rocket::Core::ElementDocument *doc) {
	DocumentData *dd = GetDocumentData(doc);
	if (dd != NULL) {
		return dd->menu->GetNumChildren();
	}
	return NULL;
}

Rocket::Core::Element* RocketMenuPlugin::GetActiveOption(Rocket::Core::Element *menu_item) {
	OptionsData *data = GetOptionsData(menu_item);
	if (data != NULL) {
		return *(data->current);
	}
	return NULL;
}

Rocket::Core::Element* RocketMenuPlugin::GetOptionById(Rocket::Core::Element *menu_item, const Rocket::Core::String& option_id) {
	OptionsData *data = GetOptionsData(menu_item);
	if (data != NULL) {
		for (Rocket::Core::ElementList::iterator i = data->options.begin(); i != data->options.end(); i++) {
			if ((*i)->GetId() == option_id) {
				return *i;
			}
		}
	}
	return NULL;
}

void RocketMenuPlugin::SetRangeValue(Rocket::Core::Element *menu_item, float value, bool notify_delegate) {
	RangeData *data = GetRangeData(menu_item);
	if (data != NULL) {
        Rocket::Core::String tmp;
//        printf("=======\n");
//        data->range->GetAttribute("min")->GetInto(tmp);
//        printf("min: %s\n", tmp.CString());
//        data->range->GetAttribute("max")->GetInto(tmp);
//        printf("max: %s\n", tmp.CString());
//        data->range->GetAttribute("value")->GetInto(tmp);
//        printf("value: %s\n", tmp.CString());
//        data->range->GetAttribute("step")->GetInto(tmp);
//        printf("step: %s\n", tmp.CString());
        data->value = clamp(value, data->min, data->max);
        
        data->range->SetAttribute("value", (data->value - data->min)/(data->max - data->min));
#if 0
		float position = data->rail_x + data->value/(data->max-data->min)*data->rail_length;
		data->grip->SetProperty("left", Rocket::Core::Property(position, Rocket::Core::Property::PX));
#endif
//		if (m_delegate != NULL && notify_delegate) {
//			m_delegate->DidChangeRangeValue(menu_item, value);
//		}
	}
}

float RocketMenuPlugin::GetRangeValue(Rocket::Core::Element *menu_item) {
	RangeData *data = GetRangeData(menu_item);
	if (data != NULL) {
		return data->value;
	}
	return 0.0f;
}

void RocketMenuPlugin::IncreaseRangeValue(Rocket::Core::Element *menu_item) {
	RangeData *data = GetRangeData(menu_item);
	if (data != NULL) {
		SetRangeValue(menu_item, data->value + data->step);
	}
}

void RocketMenuPlugin::DecreaseRangeValue(Rocket::Core::Element *menu_item) {
	RangeData *data = GetRangeData(menu_item);
	if (data != NULL) {
		SetRangeValue(menu_item, data->value - data->step);
	}
}

static
void ChangeActiveKeySlot(Rocket::Core::Element *menu_item) {
    Rocket::Core::Context *context = menu_item->GetContext();
    DocumentData *document_data = GetDocumentData(menu_item->GetOwnerDocument());
    KeyData *key_data = GetKeyData(menu_item);
    if (document_data != NULL && key_data != NULL) {
        document_data->active_key = 1-document_data->active_key;
        UpdateKeyChooser(key_data, document_data->active_key, true);
    }
}

static
void SetActiveKeySlot(Rocket::Core::Element *menu_item, int active_key) {
    Rocket::Core::Context *context = menu_item->GetContext();
    DocumentData *document_data = GetDocumentData(menu_item->GetOwnerDocument());
    KeyData *key_data = GetKeyData(menu_item);
    if (document_data != NULL && key_data != NULL) {
        document_data->active_key = active_key;
        UpdateKeyChooser(key_data, document_data->active_key, true);
    }
}

void RocketMenuPlugin::SetNextItemValue(Rocket::Core::Element *menu_item) {
	ActivateNextOption(menu_item);
	IncreaseRangeValue(menu_item);
	ChangeActiveKeySlot(menu_item);
}

void RocketMenuPlugin::SetPreviousItemValue(Rocket::Core::Element *menu_item) {
    if (menu_item != NULL) {
        ActivatePreviousOption(menu_item);
        DecreaseRangeValue(menu_item);
        ChangeActiveKeySlot(menu_item);
    }
}

void RocketMenuPlugin::RequestKeyForKeyChooser(Rocket::Core::Element *menu_item) {
//    Rocket::Core::Context *context = menu_item->GetContext();
    DocumentData *document_data = GetDocumentData(menu_item->GetOwnerDocument());
    KeyData *key_data = GetKeyData(menu_item);
    if (document_data != NULL && key_data != NULL && m_delegate != NULL) {
        m_delegate->DidRequestKey(menu_item, document_data->active_key);
    }
}

void RocketMenuPlugin::SetKeyChooserValue(Rocket::Core::Element *menu_item, int slot, const Rocket::Core::String& text, const Rocket::Core::String& id) {
    DocumentData *document_data = GetDocumentData(menu_item->GetOwnerDocument());
    KeyData *key_data = GetKeyData(menu_item);
    if (document_data != NULL && key_data != NULL && m_delegate != NULL) {
        Rocket::Core::Element *slot_element = slot ? key_data->opt2 : key_data->opt1;
        slot_element->SetId(id);
        slot_element->SetInnerRML(text);
        slot_element->SetClass("no-key-chosen", id == "None");
    }
}

const Rocket::Core::String RocketMenuPlugin::GetKeyChooserValue(Rocket::Core::Element *menu_item, int slot) {
    DocumentData *document_data = GetDocumentData(menu_item->GetOwnerDocument());
    KeyData *key_data = GetKeyData(menu_item);
    if (document_data != NULL && key_data != NULL && m_delegate != NULL) {
        Rocket::Core::Element *slot_element = slot ? key_data->opt2 : key_data->opt1;
        return slot_element->GetId();
    }
    return "";
}

void RocketMenuPlugin::ClearMenuItem(Rocket::Core::Element *menu_item) {
    KeyData *key_data = GetKeyData(menu_item);
    if (key_data != NULL) {
        DocumentData *document_data = GetDocumentData(menu_item->GetOwnerDocument());
        if (document_data != NULL) {
            SetKeyChooserValue(menu_item, document_data->active_key, "None", "None");
            if (m_delegate != NULL) {
                m_delegate->DidClearKeyChooserValue(menu_item, document_data->active_key);
            }
        }
    } else {
        if (m_delegate != NULL) {
            m_delegate->DidClearItem(menu_item);
        }
    }
}

void RocketMenuPlugin::RefreshMenuItem(Rocket::Core::Element *menu_item) {
    if (m_delegate != NULL) {
        m_delegate->DidRefreshItem(menu_item);
    }
}

void RocketMenuPlugin::ResetMenuItem(Rocket::Core::Element *menu_item) {
    if (m_delegate != NULL) {
        m_delegate->DidResetItem(menu_item);
    }
}
