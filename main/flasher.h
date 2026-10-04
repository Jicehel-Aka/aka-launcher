// flasher.h - installation d'une application (firmware.bin) dans la partition
// app0 puis demarrage dessus.
#pragma once
#include <cstddef>
#include <cstdint>

namespace launcher {

enum class FlashResult {
    Ok,
    NoTargetPartition,   // pas de partition app / OTA_0
    TargetIsRunning,     // on tournerait sur la partition qu'on veut ecrire
    OpenFailed,
    NotAnImage,          // l'en-tete n'est pas celui d'un binaire ESP32
    TooBig,
    BeginFailed,         // esp_ota_begin
    ReadFailed,
    WriteFailed,
    ValidationFailed,    // esp_ota_end : somme de controle / hash incorrect
    SetBootFailed,
    NotApplicable,       // rien a faire
};

// Details lisibles de la derniere erreur (plusieurs lignes de <= 40 caracteres) :
// etape, code esp_err, nom du code, octets ecrits, resultat de la relecture flash.
// Egalement ecrits dans le journal serie (ESP_LOGE, tag "flasher").
const char* lastErrorDetail();

// Cle de traduction du message (voir i18n.cpp) ; "OK" si tout s'est bien passe.
const char* errorKey(FlashResult r);

// progress(ecrit, total, ctx) est appelee apres chaque bloc.
using ProgressFn = void (*)(size_t written, size_t total, void* ctx);

// Ecrit le fichier dans la partition de l'application (OTA_0) et la
// selectionne pour le prochain demarrage. Ne redemarre pas.
FlashResult flashApp(const char* firmwarePath, ProgressFn progress, void* ctx);

// Si le Launcher tourne dans app0 (cas d'un flash direct depuis VS Code), se
// copie dans app1 puis la selectionne : app0 reste libre pour les jeux.
// Retourne Ok (copie faite : appeler restart()), ou un code d'erreur.
// NotApplicable si on ne tourne pas dans app0 (rien a faire).
bool runningFromApp0();
FlashResult installSelfToApp1(ProgressFn progress, void* ctx);

// Selectionne la partition application (OTA_0) pour le prochain demarrage si
// elle contient une image valide. Retourne false sinon.
bool selectApplication();

// Redemarre (ne retourne pas).
[[noreturn]] void restart();

}  // namespace launcher
