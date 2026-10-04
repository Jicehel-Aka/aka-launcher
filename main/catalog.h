// catalog.h - inventaire des applications presentes sur la carte SD.
//
// Convention (identique a l'App Store AKA) : un dossier par application, a la
// racine de la carte :
//     /sdcard/<dossier>/firmware.bin   binaire ESP32-S3 (obligatoire)
//     /sdcard/<dossier>/meta.json      {title, description, author, version, date}
//     /sdcard/<dossier>/screen.bmp     apercu BMP RGB565 4:3 (~160x120) ; screen.bpm accepte aussi
#pragma once
#include <cstddef>
#include <cstdint>
#include "launcher_config.h"

namespace launcher {

struct AppEntry {
    char     folder[64];                 // nom du dossier (identifiant stable)
    char     title[kMaxTextLen];
    char     author[kMaxTextLen];
    char     version[32];
    char     date[32];
    char     description[kMaxDescLen];
    uint32_t firmwareSize;               // octets
    bool     hasPreview;
};

class Catalog {
public:
    // Parcourt la carte. Retourne le nombre d'applications trouvees, ou -1 si
    // la carte n'est pas lisible.
    int scan();

    size_t          count() const { return m_count; }
    const AppEntry& at(size_t i) const { return m_apps[i]; }
    int             indexOf(const char* folder) const;

    // Chemin complet d'un fichier de l'application i ("firmware.bin"...).
    void path(size_t i, const char* file, char* out, size_t cap) const;

private:
    AppEntry m_apps[kMaxApps];
    size_t   m_count = 0;
};

// Remplace les caracteres UTF-8 accentues par leur equivalent ASCII (la police
// 8x8 de la bibliotheque ne contient que l'ASCII).
void asciiFold(const char* utf8, char* out, size_t cap);

}  // namespace launcher
