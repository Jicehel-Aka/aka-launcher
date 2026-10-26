// Launcher Gamebuino AKA
//
// Runs from the "test" partition (0x10000). Lists the applications on the SD card
// (one folder per application), installs the chosen one into the "factory"
// partition (0x100000) and restarts into it.
//
// Touches :
//   HAUT / BAS     naviguer             L1 / R1       page precedente / suivante
//   A              installer et lancer  D             relire la carte SD
//   C              aide                 MENU          options (langue)
//   L1 + R1        relancer la derniere application installee
// (les applications reviennent ici avec [HOME]+[MENU] maintenus 500 ms.)
#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "gamebuino.h"

#include "breakout.h"
#include "catalog.h"
#include "flasher.h"
#include "i18n.h"
#include "settings.h"
#include "ui.h"

gb_core     g_core;
gb_graphics gfx;
#if USE_WIFI_STREAM
gb_stream_wifi      stream_wifi;
#endif

using namespace launcher;

namespace {


EXT_RAM_BSS_ATTR Catalog g_catalog;
EXT_RAM_BSS_ATTR  Ui      g_ui;

constexpr uint16_t kAnyKey = GB_KEY_RUN | GB_KEY_MENU | GB_KEY_L1 | GB_KEY_R1 | GB_KEY_UP | GB_KEY_DOWN |
                             GB_KEY_LEFT | GB_KEY_RIGHT | GB_KEY_A | GB_KEY_B | GB_KEY_C | GB_KEY_D;

// Etat des touches de la frame : croix + joystick (emulation de croix).
uint16_t held()    { return g_core.buttons.state()   | g_core.joystick.state(); }
uint16_t pressed() { return g_core.buttons.pressed() | g_core.joystick.pressed(); }

void showMessage(const char* title, const char* text, bool error)
{
    g_ui.drawMessage(title, text, error);
    gfx.update();
#if USE_WIFI_STREAM
    stream_wifi.update();
#endif
    // attendre le relachement puis une nouvelle pression
    while (true) { g_core.pool(); if (!held()) break; g_core.delay_ms(10); }
    while (true) { g_core.pool(); if (pressed() & kAnyKey) break; g_core.delay_ms(10); }
}

bool mountCard()
{
//    if (gb_ll_sd_is_mounted()) return true;
//    return gb_ll_sd_init() == GB_OK;
    return true;
}

// Rescanne la carte ; retourne true si elle est lisible.
bool rescan(int* selected)
{
    const bool ok = mountCard() && g_catalog.scan() >= 0;
    i18nScan();                                  // langues de la SD (+ integrees)
    if (!ok) { *selected = -1; return false; }
    if (*selected >= (int)g_catalog.count()) *selected = (int)g_catalog.count() - 1;
    if (*selected < 0 && g_catalog.count() > 0) *selected = 0;
    return true;
}

// Message d'erreur d'installation : texte traduit + details techniques (etape, code, relecture
// de la flash) pour pouvoir diagnostiquer sans cable serie.
void showFlashError(FlashResult r)
{
    char msg[320];
    snprintf(msg, sizeof msg, "%s\n \n%s", tr(errorKey(r)), lastErrorDetail());
    showMessage(tr("LAUNCHER_ERROR"), msg, true);
}

// Launcher installed in "test": ask the user to power off, then wait forever.
[[noreturn]] void waitPowerOff()
{
    g_ui.drawMessage(tr("LAUNCHER_DONE"), tr("LAUNCHER_POWER_OFF"), false);
    gfx.update();
    while (true)
    { 
        g_core.pool(); // update buttons, include power off ;)
        g_core.delay_ms(10);
    }
}

// Fin d'une auto-installation : disposition loader/game = eteindre puis rallumer (la mise sous
// tension demarre le loader) ; disposition OTA = redemarrer directement sur le loader.
[[noreturn]] void finishSelfInstall()
{
    if (selfInstallNeedsPowerOff()) waitPowerOff();
    g_ui.drawMessage(tr("LAUNCHER_DONE"), tr("LAUNCHER_STARTING"), false);
    gfx.update();
    g_core.delay_ms(600);
    restartToLoaderNow();
}

void onProgress(size_t done, size_t total, void* ctx)
{
    g_ui.drawProgress((const char*)ctx, done, total);
    gfx.update();
#if USE_WIFI_STREAM
    stream_wifi.update();
#endif
}

void install(int index)
{
    const AppEntry& app = g_catalog.at(index);
    char path[320];
    g_catalog.path(index, kFirmwareName, path, sizeof path);

    char title[kMaxTextLen];
    snprintf(title, sizeof title, "%s", app.title);
    onProgress(0, app.firmwareSize ? app.firmwareSize : 1, title);

    const FlashResult r = flashApp(path, onProgress, title);
    if (r != FlashResult::Ok) {
        showFlashError(r);
        return;
    }
    setLastApp(app.folder);
    g_ui.drawMessage(tr("LAUNCHER_DONE"), tr("LAUNCHER_STARTING"), false);
    gfx.update();
#if USE_WIFI_STREAM
    stream_wifi.update();
#endif
    g_core.delay_ms(600);
    restartToGame();
}

// Aide : gauche/droite change de page, B / C / MENU referme.
void helpScreen()
{
    constexpr int kPages = 3;
    int page = 0;
    while (true) {
        g_core.pool();
        const uint16_t edge = pressed();
        if (edge & GB_KEY_RIGHT) page = (page + 1) % kPages;
        if (edge & GB_KEY_LEFT)  page = (page + kPages - 1) % kPages;
        if (edge & (GB_KEY_B | GB_KEY_C | GB_KEY_MENU)) break;
        g_ui.drawHelp(page, kPages);
        gfx.update();
    #if USE_WIFI_STREAM
        stream_wifi.update();
    #endif
    }
}

// Ecran des options : gauche/droite change la langue (appliquee tout de suite),
// B ou MENU enregistre et revient.
void optionsScreen()
{
    int idx = languageIndex(currentLanguage());
    if (idx < 0) idx = 0;
    const int n = (int)languageCount();
    while (true) {
        g_core.pool();
        const uint16_t edge = pressed();
        int step = 0;
        if (edge & GB_KEY_RIGHT) step = 1;
        if (edge & GB_KEY_LEFT)  step = -1;
        if (step && n > 0) {
            idx = (idx + step + n) % n;
            setCurrentLanguage(languageAt(idx).code);
        }
        if (edge & (GB_KEY_B | GB_KEY_MENU)) break;
        if ((held() & GB_KEY_L1) && (held() & GB_KEY_R1)) { if (hasApplication()) restartToGame(); }
        g_ui.drawOptions(idx);
        gfx.update();
    #if USE_WIFI_STREAM
        stream_wifi.update();
    #endif
    }
    if (n > 0) setLanguage(languageAt(idx).code);
}

}  // namespace

extern "C" void app_main(void)
{
    g_core.init();
    gfx.set_refresh_rate(100);
    /*
        // check reset reason
    {
        esp_reset_reason_t reset_reason = esp_reset_reason();
        char log_boot[30];
        snprintf( log_boot, sizeof(log_boot), "code %d", reset_reason );
        g_ui.drawMessage( "BOOT CODE", log_boot, false);
        gfx.update();
        g_core.delay_ms(2000);
    }*/

    settingsInit();
    {   // langue enregistree (SD, sinon NVS) avant le premier affichage
        char lang[8];
        if (mountCard() && getLanguage(lang, sizeof lang)) setCurrentLanguage(lang);
        else if (getLanguage(lang, sizeof lang))           setCurrentLanguage(lang);
    }

    // Flashed by idf.py (VS Code), the Launcher lands in "factory" like any game:
    // copy it into "test" so "factory" stays free for games, then ask for a power cycle
    // (power-on starts "test").
    if (runningAsGame()) {
        // Same image already in "test" (e.g. reset while the copy was still in "factory"):
        // no need to flash it again.
        if (compareLoader() == LoaderMatch::Same) finishSelfInstall();

        char title[kMaxTextLen];
        snprintf(title, sizeof title, "%s", tr("LAUNCHER_SELF_TITLE"));
        onProgress(0, 1, title);
        const FlashResult r = installSelfToLoader(onProgress, title);
        if (r == FlashResult::Ok) finishSelfInstall();
        showFlashError(r);
    }
#if USE_WIFI_STREAM
    stream_wifi.init(&g_core, &gfx);
    stream_wifi.connect_ap();
#endif


    int selected = 0, top = 0;
    bool sdOk = rescan(&selected);

    // Replace la selection sur la derniere application installee.
    char last[64];
    if (sdOk && getLastApp(last, sizeof last)) {
        const int i = g_catalog.indexOf(last);
        if (i >= 0) selected = i;
    }
    int shownPreview = -2;

    Breakout game;
    uint32_t nextGameTick = 0;
    uint32_t nextRepeat = 0;
    uint16_t lastDir = 0;

    while (true) {
        g_core.pool();
        const uint16_t now = held(), edge = pressed();
        const int n = (int)g_catalog.count();

        // L1 + R1 : derniere application.
        if ((now & GB_KEY_L1) && (now & GB_KEY_R1)) {
            if (hasApplication()) restartToGame();
            showMessage(tr("LAUNCHER_NOAPP_TITLE"), tr("LAUNCHER_NOAPP_MSG"), true);
            continue;
        }

        // [MENU] : options (langue).
        if (edge & GB_KEY_MENU) {
            optionsScreen();
            shownPreview = -2;
            continue;
        }

        // [C] : aide.
        if (edge & GB_KEY_C) {
            helpScreen();
            continue;
        }

        // [D] : relire la carte SD.
        if (edge & GB_KEY_D) {
            sdOk = rescan(&selected);
            shownPreview = -2;
        }

        // Navigation avec repetition (400 ms puis 80 ms).
        const uint16_t dir = now & (GB_KEY_UP | GB_KEY_DOWN);
        const uint32_t t = g_core.get_millis();
        int step = 0;
        if (dir && (dir != lastDir || t >= nextRepeat)) {
            step = (dir & GB_KEY_UP) ? -1 : 1;
            nextRepeat = t + (dir != lastDir ? 400 : 80);
        }
        lastDir = dir;
        if (edge & GB_KEY_L1) step = -Ui::kVisibleRows;       // page
        if (edge & GB_KEY_R1) step = Ui::kVisibleRows;
        if (n > 0 && step) {
            selected += step;
            if (step == 1 || step == -1) {                   // defilement circulaire
                if (selected < 0) selected = n - 1;
                if (selected >= n) selected = 0;
            } else {                                          // pages : on bute aux extremites
                if (selected < 0) selected = 0;
                if (selected >= n) selected = n - 1;
            }
        }

        // [A] : installer.
        if ((edge & GB_KEY_A) && sdOk && selected >= 0 && selected < n) {
            install(selected);
            shownPreview = -2;
        }

        // Fenetre de defilement.
        if (selected < top) top = selected;
        if (selected >= top + Ui::kVisibleRows) top = selected - Ui::kVisibleRows + 1;
        if (top < 0) top = 0;

        if (selected != shownPreview) {
            g_ui.loadPreview(g_catalog, selected);
            shownPreview = selected;
        }

        if (!sdOk || n == 0) {
            // Rien a lancer : message bien visible + casse-brique (pas fixe de 16 ms).
            const uint32_t tg = g_core.get_millis();
            if (tg >= nextGameTick) {
                game.step((now & GB_KEY_RIGHT ? 1 : 0) - (now & GB_KEY_LEFT ? 1 : 0), (edge & GB_KEY_A) != 0);
                nextGameTick = tg + 16;
            }
            g_ui.drawErrorGame(game, sdOk);
        } else {
            g_ui.drawBrowser(g_catalog, selected, top, sdOk);
        }
        gfx.update();
    #if USE_WIFI_STREAM
        stream_wifi.update();
    #endif
    }
}
