// flasher.h - installe une application (firmware.bin) et passe du loader au jeu.
//
// Deux dispositions de la flash sont reconnues, par type/sous-type (aucun offset en dur) :
//
//  - RESET_REASON (bootloader modifie, voir bootloader_components/main/bootloader_start.c)
//      loader  app/test     le Launcher
//      game    app/factory  l'application installee
//    Pas d'otadata : le bootloader choisit la partition selon la cause du reset
//    (mise sous tension ou reset watchdog RTC -> loader, tout autre reset -> game).
//
//  - OTA (ancienne organisation de la console)
//      app1  app/ota_1   le Launcher
//      app0  app/ota_0   l'application installee
//    La partition de demarrage est choisie par otadata (esp_ota_set_boot_partition).
#pragma once
#include <cstddef>
#include <cstdint>

namespace launcher {

enum class Layout {
    None,         // aucune des deux dispositions
    ResetReason,  // loader (test) + game (factory)
    Ota,          // app1 (ota_1) + app0 (ota_0)
};

enum class FlashResult {
    Ok,
    NoTargetPartition,   // pas de partition "jeu" / "loader" dans la table
    TargetIsRunning,     // la partition a ecrire est celle d'ou l'on tourne
    OpenFailed,
    NotAnImage,          // l'en-tete n'est pas celui d'un binaire ESP32
    TooBig,
    BeginFailed,         // allocation du tampon impossible
    ReadFailed,
    WriteFailed,
    ValidationFailed,    // somme de controle / SHA-256 de l'image ecrite incorrect
    SetBootFailed,       // OTA : selection de la partition de demarrage
    NotApplicable,       // rien a faire
};

Layout layout();

// Cle de traduction du message (voir i18n.cpp) ; "OK" si tout s'est bien passe.
const char* errorKey(FlashResult r);

// Details techniques de la derniere operation (etape, code esp_err, octets ecrits, relecture
// de la flash...) : a afficher sous le message d'erreur et envoye sur la liaison serie.
const char* lastErrorDetail();

// progress(ecrit, total, ctx) est appelee apres chaque bloc.
using ProgressFn = void (*)(size_t written, size_t total, void* ctx);

// Ecrit le fichier dans la partition du jeu et le verifie. Ne redemarre pas.
FlashResult flashApp(const char* firmwarePath, ProgressFn progress, void* ctx);

// Vrai si l'on tourne depuis la partition du jeu : le Launcher vient d'etre flashe par idf.py.
bool runningAsGame();

// Copie le Launcher en cours (partition du jeu) dans la partition loader.
// Retourne Ok, NotApplicable si l'on ne tourne pas depuis la partition du jeu, ou une erreur.
FlashResult installSelfToLoader(ProgressFn progress, void* ctx);

// Apres une auto-installation reussie : vrai si la console doit etre eteinte puis rallumee
// (disposition ResetReason), faux si finishSelfInstall() peut redemarrer directement (OTA).
bool selfInstallNeedsPowerOff();

// OTA : selectionne le loader et redemarre dessus. Ne retourne pas.
[[noreturn]] void restartToLoaderNow();

enum class LoaderMatch {
    Same,        // la partition du jeu contient exactement l'image installee dans loader
    Different,   // image differente, ou pas d'image valide dans la partition du jeu
    Error,       // partition absente, allocation ou erreur de lecture
};

// Comparaison binaire de l'image de la partition du jeu avec le debut de loader.
LoaderMatch compareLoader();

// Vrai si la partition du jeu contient une image valide.
bool hasApplication();

// Redemarre sur le jeu (OTA : le selectionne d'abord). Ne retourne pas.
[[noreturn]] void restartToGame();

}  // namespace launcher
