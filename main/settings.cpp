#include "settings.h"

#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "launcher_config.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace launcher {

namespace {
constexpr const char* kNamespace = "launcher";
constexpr const char* kKeyLast   = "lastApp";
constexpr const char* kKeyLang   = "lang";
constexpr const char* kJsonPath  = "/sdcard/AKA/settings.json";

// Valeur d'une cle chaine / numerique dans le petit JSON plat des reglages.
bool jsonStr(const char* j, const char* key, char* out, size_t cap)
{
    char pat[32]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p || !(p = strchr(p + strlen(pat), ':')) || !(p = strchr(p, '"'))) return false;
    ++p;
    size_t n = 0;
    while (*p && *p != '"' && n + 1 < cap) out[n++] = *p++;
    out[n] = '\0';
    return n > 0;
}

int jsonInt(const char* j, const char* key, int def)
{
    char pat[32]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p || !(p = strchr(p + strlen(pat), ':'))) return def;
    return atoi(p + 1);
}
}

void settingsInit()
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

bool getLastApp(char* folder, size_t cap)
{
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = cap;
    const bool ok = nvs_get_str(h, kKeyLast, folder, &len) == ESP_OK;
    nvs_close(h);
    return ok;
}

void setLastApp(const char* folder)
{
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, kKeyLast, folder);
    nvs_commit(h);
    nvs_close(h);
}

bool getLanguage(char* code, size_t cap)
{
    if (FILE* f = fopen(kJsonPath, "rb")) {
        char buf[256];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = '\0';
        if (jsonStr(buf, "language", code, cap)) return true;
    }
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = cap;
    const bool ok = nvs_get_str(h, kKeyLang, code, &len) == ESP_OK;
    nvs_close(h);
    return ok;
}

void setLanguage(const char* code)
{
    // copie en NVS (toujours disponible)
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, kKeyLang, code);
        nvs_commit(h);
        nvs_close(h);
    }

    // fichier partage avec les applications : on garde les volumes en place
    int music = 80, sfx = 70;
    if (FILE* f = fopen(kJsonPath, "rb")) {
        char buf[256];
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = '\0';
        music = jsonInt(buf, "musicVolume", music);
        sfx   = jsonInt(buf, "sfxVolume", sfx);
    }
    mkdir("/sdcard/AKA", 0777);
    if (FILE* f = fopen(kJsonPath, "wb")) {
        fprintf(f, "{\n  \"language\": \"%s\",\n  \"musicVolume\": %d,\n  \"sfxVolume\": %d\n}\n",
                code, music, sfx);
        fclose(f);
    }
}

}  // namespace launcher
