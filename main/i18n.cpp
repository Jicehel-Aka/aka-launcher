#include "i18n.h"

#include <dirent.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "catalog.h"          // asciiFold
#include "launcher_config.h"

namespace launcher {

namespace {

struct Entry { const char* key; const char* fr; const char* en; };

// Textes integres (ASCII : la police 8x8 n'a pas les accents).
const Entry kBuiltin[] = {
    {"LAUNCHER_TITLE",       "GAMEBUINO AKA - LAUNCHER",         "GAMEBUINO AKA - LAUNCHER"},
    {"LAUNCHER_APPS_COUNT",  "%u app.",                          "%u apps"},
    {"LAUNCHER_SD_MISSING1", "Carte SD absente",                 "SD card missing"},
    {"LAUNCHER_SD_MISSING2", "ou illisible.",                    "or unreadable."},
    {"LAUNCHER_SD_RETRY",    "[D] pour reessayer",               "[D] to retry"},
    {"LAUNCHER_NO_APP1",     "Aucune application.",              "No application."},
    {"LAUNCHER_NO_APP2",     "Un dossier par appli,",            "One folder per app,"},
    {"LAUNCHER_NO_APP3",     "avec firmware.bin",                "with firmware.bin"},
    {"LAUNCHER_NO_PREVIEW",  "(pas d'apercu)",                   "(no preview)"},
    {"LAUNCHER_BY",          "par",                              "by"},
    {"LAUNCHER_FOOTER",      "A:Install. D:Relire C:Aide MENU:Options","A:Install D:Reload C:Help MENU:Options"},
    {"LAUNCHER_HELP_TITLE",  "AIDE",                             "HELP"},
    {"LAUNCHER_HELP_NAV",    "< > page   B : fermer",            "< > page   B: close"},
    {"LAUNCHER_HELP1_T",     "Utilisation",                      "Usage"},
    {"LAUNCHER_HELP1",       "Haut/Bas   choisir une application\n"
                             "L1 / R1    page precedente/suivante\n"
                             "A          installer et lancer\n"
                             "D          relire la carte SD\n"
                             "C          cette aide\n"
                             "MENU       options (langue)\n"
                             "L1 + R1    relancer la derniere\n"
                             "           application installee",
                             "Up/Down    pick an application\n"
                             "L1 / R1    previous/next page\n"
                             "A          install and launch\n"
                             "D          reload the SD card\n"
                             "C          this help\n"
                             "MENU       options (language)\n"
                             "L1 + R1    launch the last\n"
                             "           installed application"},
    {"LAUNCHER_HELP2_T",     "Carte SD",                         "SD card"},
    {"LAUNCHER_HELP2",       "Un dossier par application :\n"
                             "  /MonJeu/firmware.bin   requis\n"
                             "  /MonJeu/meta.json      fiche\n"
                             "  /MonJeu/screen.bmp     apercu\n"
                             "\n"
                             "meta.json : title, description,\n"
                             "author, version, date.\n"
                             "Apercu : BMP RGB565 4:3, ~160x120.",
                             "One folder per application:\n"
                             "  /MyGame/firmware.bin   required\n"
                             "  /MyGame/meta.json      info\n"
                             "  /MyGame/screen.bmp     preview\n"
                             "\n"
                             "meta.json: title, description,\n"
                             "author, version, date.\n"
                             "Preview: BMP RGB565 4:3, ~160x120."},
    {"LAUNCHER_HELP3_T",     "Bon a savoir",                     "Good to know"},
    {"LAUNCHER_HELP3",       "HOME + MENU (500 ms) dans une\n"
                             "application : retour au Launcher.\n"
                             "\n"
                             "Ne pas eteindre pendant une\n"
                             "installation.\n"
                             "\n"
                             "Autres langues : fichiers\n"
                             "/AKA/lang/<code>.json sur la SD.",
                             "HOME + MENU (500 ms) inside an\n"
                             "application: back to the Launcher.\n"
                             "\n"
                             "Do not power off during an\n"
                             "installation.\n"
                             "\n"
                             "More languages: files\n"
                             "/AKA/lang/<code>.json on the SD."},
    {"LAUNCHER_INSTALLING",  "INSTALLATION",                     "INSTALLING"},
    {"LAUNCHER_KEEP_POWER",  "Ne pas eteindre la console",       "Do not power off the console"},
    {"LAUNCHER_DONE",        "INSTALLATION TERMINEE",            "INSTALL COMPLETE"},
    {"LAUNCHER_STARTING",    "Demarrage...",                     "Starting..."},
    {"LAUNCHER_ERROR",       "ERREUR",                           "ERROR"},
    {"LAUNCHER_NOAPP_TITLE", "AUCUNE APPLICATION",               "NO APPLICATION"},
    {"LAUNCHER_NOAPP_MSG",   "app0 est vide.\nInstallez-en une avec [A].",
                             "app0 is empty.\nInstall one with [A]."},
    {"LAUNCHER_PRESS_KEY",   "Appuyez sur une touche",           "Press any key"},
    {"LAUNCHER_OPT_TITLE",   "OPTIONS",                          "OPTIONS"},
    {"LAUNCHER_OPT_LANG",    "Langue",                           "Language"},
    {"LAUNCHER_OPT_HELP1",   "Gauche/Droite : changer",          "Left/Right: change"},
    {"LAUNCHER_OPT_HELP2",   "B ou MENU : retour",               "B or MENU: back"},
    {"LAUNCHER_OPT_HELP3",   "L1+R1 : derniere application",     "L1+R1: launch last application"},
    {"LAUNCHER_SELF_TITLE",  "Installation du Launcher",         "Installing the Launcher"},
    {"LAUNCHER_SELF_MSG",    "Copie dans app1 (app0 reste libre)","Copying to app1 (app0 stays free)"},
    {"LAUNCHER_ERR_NO_PART", "Partition app0 introuvable",       "app0 partition not found"},
    {"LAUNCHER_ERR_RUNNING", "Partition cible en cours d'usage", "Target partition is running"},
    {"LAUNCHER_ERR_OPEN",    "Lecture de firmware.bin impossible","Cannot read firmware.bin"},
    {"LAUNCHER_ERR_IMAGE",   "Pas un binaire ESP32-S3",          "Not an ESP32-S3 binary"},
    {"LAUNCHER_ERR_BIG",     "Application trop grande pour app0","Application too big for app0"},
    {"LAUNCHER_ERR_BEGIN",   "Preparation de la flash impossible","Cannot prepare flash"},
    {"LAUNCHER_ERR_READ",    "Erreur de lecture de la carte SD", "SD card read error"},
    {"LAUNCHER_ERR_WRITE",   "Erreur d'ecriture en flash",       "Flash write error"},
    {"LAUNCHER_ERR_VALID",   "Image invalide (integrite)",       "Invalid image (integrity check)"},
    {"LAUNCHER_ERR_BOOT",    "Selection du demarrage impossible","Cannot select boot partition"},
};

// Noms d'affichage des codes les plus courants.
struct Known { const char* code; const char* name; };
const Known kKnown[] = {
    {"fr", "Francais"}, {"en", "English"}, {"es", "Espanol"}, {"de", "Deutsch"},
    {"it", "Italiano"}, {"pt", "Portugues"}, {"nl", "Nederlands"}, {"ca", "Catala"},
    {"pl", "Polski"},   {"sv", "Svenska"},
};

constexpr size_t kMaxOverrides = 40;
constexpr size_t kKeyLen = 24 + 16, kValLen = 320;

Language g_langs[kMaxLanguages];
size_t   g_langCount = 0;
char     g_current[8] = "fr";

char     g_okeys[kMaxOverrides][kKeyLen];
char     g_ovals[kMaxOverrides][kValLen];
size_t   g_overrides = 0;

void addLanguage(const char* code)
{
    if (g_langCount >= kMaxLanguages || strlen(code) == 0 || strlen(code) >= sizeof(Language::code)) return;
    for (size_t i = 0; i < g_langCount; ++i)
        if (strcasecmp(g_langs[i].code, code) == 0) return;
    Language& l = g_langs[g_langCount++];
    snprintf(l.code, sizeof l.code, "%s", code);
    snprintf(l.name, sizeof l.name, "%s", code);
    for (const Known& k : kKnown)
        if (strcasecmp(k.code, code) == 0) { snprintf(l.name, sizeof l.name, "%s", k.name); return; }
    for (char* p = l.name; *p; ++p) *p = (char)toupper((unsigned char)*p);   // code inconnu
}

// Charge les paires "LAUNCHER_xxx": "valeur" du fichier de langue.
void loadOverrides(const char* code)
{
    g_overrides = 0;
    char path[96];
    snprintf(path, sizeof path, "%s/AKA/lang/%s.json", kSdRoot, code);
    FILE* f = fopen(path, "rb");
    if (!f) return;
    static char buf[8192];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';

    const char* p = buf;
    while (*p && g_overrides < kMaxOverrides) {
        while (*p && *p != '"') ++p;
        if (!*p) break;
        const char* ks = ++p;
        while (*p && *p != '"') ++p;
        if (!*p) break;
        const size_t kl = (size_t)(p - ks);
        ++p;
        while (*p && *p != ':') ++p;
        if (!*p) break;
        ++p;
        while (*p && *p != '"') ++p;
        if (!*p) break;
        const char* vs = ++p;
        while (*p && *p != '"') { if (*p == '\\' && p[1]) ++p; ++p; }
        const size_t vl = (size_t)(p - vs);
        if (*p) ++p;

        if (kl == 0 || kl >= kKeyLen || strncmp(ks, "LAUNCHER_", 9) != 0) continue;
        char raw[kValLen * 2];
        const size_t cp = vl < sizeof raw - 1 ? vl : sizeof raw - 1;
        // dessine \n comme un vrai retour a la ligne, le reste tel quel
        size_t o = 0;
        for (size_t i = 0; i < cp; ++i) {
            if (vs[i] == '\\' && i + 1 < cp && vs[i + 1] == 'n') { raw[o++] = '\n'; ++i; }
            else if (vs[i] == '\\' && i + 1 < cp) { raw[o++] = vs[++i]; }
            else raw[o++] = vs[i];
        }
        raw[o] = '\0';
        memcpy(g_okeys[g_overrides], ks, kl);
        g_okeys[g_overrides][kl] = '\0';
        asciiFold(raw, g_ovals[g_overrides], kValLen);
        ++g_overrides;
    }
}

}  // namespace

void i18nScan()
{
    g_langCount = 0;
    addLanguage("fr");
    addLanguage("en");

    char dir[64];
    snprintf(dir, sizeof dir, "%s/AKA/lang", kSdRoot);
    if (DIR* d = opendir(dir)) {
        while (struct dirent* e = readdir(d)) {
            const char* dot = strrchr(e->d_name, '.');
            if (!dot || strcasecmp(dot, ".json") != 0 || dot == e->d_name) continue;
            char code[8];
            const size_t n = (size_t)(dot - e->d_name);
            if (n >= sizeof code) continue;
            memcpy(code, e->d_name, n);
            code[n] = '\0';
            addLanguage(code);
        }
        closedir(d);
    }
    loadOverrides(g_current);
}

size_t          languageCount()          { return g_langCount; }
const Language& languageAt(size_t i)     { return g_langs[i]; }

int languageIndex(const char* code)
{
    for (size_t i = 0; i < g_langCount; ++i)
        if (strcasecmp(g_langs[i].code, code) == 0) return (int)i;
    return -1;
}

const char* currentLanguage() { return g_current; }

void setCurrentLanguage(const char* code)
{
    snprintf(g_current, sizeof g_current, "%s", code);
    loadOverrides(g_current);
}

const char* tr(const char* key)
{
    for (size_t i = 0; i < g_overrides; ++i)
        if (strcmp(g_okeys[i], key) == 0) return g_ovals[i];
    for (const Entry& e : kBuiltin) {
        if (strcmp(e.key, key) != 0) continue;
        return strcasecmp(g_current, "fr") == 0 ? e.fr : e.en;      // autres langues : anglais
    }
    return key;
}

}  // namespace launcher
