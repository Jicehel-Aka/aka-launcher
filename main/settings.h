// settings.h - reglages du Launcher.
//  - derniere application installee : NVS (namespace "launcher")
//  - langue : /sdcard/AKA/settings.json, le MEME fichier que celui lu par les
//    applications (cles language / musicVolume / sfxVolume), avec copie en NVS
//    pour retrouver la langue quand la carte est absente.
#pragma once
#include <cstddef>

namespace launcher {

void settingsInit();                                       // nvs_flash_init
bool getLastApp(char* folder, size_t cap);
void setLastApp(const char* folder);

bool getLanguage(char* code, size_t cap);                  // false : aucun reglage enregistre
void setLanguage(const char* code);                        // conserve les volumes existants

}  // namespace launcher
