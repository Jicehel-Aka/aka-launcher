// i18n.h - langues du Launcher.
//
// Textes integres : francais (fr) et anglais (en). D'autres langues peuvent
// etre ajoutees sans recompiler : toute langue <code> dont le fichier
// /sdcard/AKA/lang/<code>.json existe apparait dans les options, et ses cles
// "LAUNCHER_*" (voir i18n.cpp pour la liste) remplacent les textes integres ;
// les cles absentes retombent sur l'anglais.
#pragma once
#include <cstddef>

namespace launcher {

constexpr size_t kMaxLanguages = 12;

struct Language {
    char code[8];        // "fr", "en", "es"...
    char name[20];       // nom affiche (ASCII)
};

// Reconstruit la liste des langues disponibles (integrees + fichiers de la SD)
// et charge les textes de la langue courante.
void i18nScan();

size_t          languageCount();
const Language& languageAt(size_t i);
int             languageIndex(const char* code);      // -1 si inconnue

// Langue courante : code, et changement (recharge les textes).
const char* currentLanguage();
void        setCurrentLanguage(const char* code);

// Texte traduit pour une cle ("LAUNCHER_TITLE"...). Ne retourne jamais null.
const char* tr(const char* key);

}  // namespace launcher
