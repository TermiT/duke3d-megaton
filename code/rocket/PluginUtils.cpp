//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#include "PluginUtils.h"

void SetElementUserData(Rocket::Core::Element *element, void *user_data, const Rocket::Core::String& key) {
	char ptr_text[10];
	sprintf(ptr_text, "%x", user_data);
	element->SetAttribute(key, ptr_text);
}

void* GetElementUserData(Rocket::Core::Element *element, const Rocket::Core::String& key) {
	void *result = NULL;
    if (element != NULL) {
        Rocket::Core::Variant *value = element->GetAttribute(key);
        if (value != NULL) {
            Rocket::Core::String strval;
            value->GetInto(strval);
            sscanf(strval.CString(), "%x", &result);
        }
    }
	return result;
}

void ShowElement(Rocket::Core::Element *element, bool visible, const Rocket::Core::String& display_mode) {
	element->SetProperty("display", visible ? display_mode : "none");
}
