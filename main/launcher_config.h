// launcher_config.h - constantes communes du Launcher AKA.
#pragma once
#include <cstddef>
#include <cstdint>

namespace launcher {

constexpr const char* kSdRoot        = "/sdcard";
constexpr const char* kFirmwareName  = "firmware.bin";
constexpr const char* kMetaName      = "meta.json";
#define USE_WIFI_STREAM 0
constexpr const char* kScreenName    = "screen.bmp";   // nom lu par le Launcher d'origine
constexpr const char* kScreenNameAlt = "screen.bpm";   // ancienne orthographe (faute de frappe), toujours acceptee

constexpr size_t kMaxApps            = 64;     // applications listees
constexpr size_t kMaxTextLen         = 96;     // titre, auteur...
constexpr size_t kMaxDescLen         = 192;
constexpr size_t kMaxPreviewW        = 160;    // apercu 4:3
constexpr size_t kMaxPreviewH        = 120;

constexpr uint32_t kComboHoldMs      = 500;    // [HOME]+[MENU] -> Launcher
constexpr size_t   kFlashChunk       = 4096;   // taille de lecture/ecriture

}  // namespace launcher
