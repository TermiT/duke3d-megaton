//
// Created by Sergei Shubin <s.v.shubin@gmail.com>
//

#ifndef PLUGIN_UTILS_H
#define PLUGIN_UTILS_H

#include <Rocket/Core.h>
#include <Rocket/Core/Element.h>

void  SetElementUserData(Rocket::Core::Element *element, void *user_data, const Rocket::Core::String& key);
void* GetElementUserData(Rocket::Core::Element *element, const Rocket::Core::String& key);

void ShowElement(Rocket::Core::Element *element, bool visible, const Rocket::Core::String& display_mode = "inline");

#endif /* PLUGIN_UTILS_H */
