# Launcher Gamebuino AKA (code neuf)

Launcher en C++ pour ESP-IDF 5.x / ESP32-S3, au-dessus de la bibliotheque
`components/gamebuino`. Il s'installe dans la partition **app1 (OTA_1)**.

## Fonctionnement
- Parcourt la racine de la carte SD : un dossier = une application si elle
  contient `firmware.bin` ; `meta.json` (title, description, author, version,
  date) et `screen.bmp` (BMP 16/24/32 bits, ~160x120 ; `screen.bpm` est aussi accepte) sont facultatifs.
- **[A]** installe l'application choisie dans **app0 (OTA_0)** (`esp_ota_*`, avec
  controle d'integrite), la selectionne pour le demarrage et redemarre dessus.
- **[D]** relit la carte SD. **[L1]+[R1]** relance la derniere application.
- Haut/Bas (ou joystick) : navigation avec repetition ; L1/R1 seuls : pages.
- La derniere application installee est memorisee en NVS (`launcher/lastApp`) et
  re-selectionnee au demarrage.
- **[C]** ouvre l'**aide integree** (3 pages : utilisation, carte SD, bon a savoir ;
  gauche/droite pour changer de page, B/C/MENU pour fermer). Textes traduisibles
  (cles `LAUNCHER_HELP*`).
- **[MENU]** ouvre les options : **gauche/droite** change la **langue** (appliquee
  immediatement), **B** ou **MENU** enregistre et revient.
- La langue est ecrite dans `/sdcard/AKA/settings.json` (cle `language`, meme
  fichier que les applications ; les volumes existants sont conserves) avec une
  copie en NVS pour le cas ou la carte est absente.
- Langues : francais et anglais integres. Toute langue dont le fichier
  `/sdcard/AKA/lang/<code>.json` existe apparait dans les options ; ses cles
  `LAUNCHER_*` (liste dans `main/i18n.cpp`) remplacent les textes integres, les
  cles absentes retombent sur l'anglais.
- Les partitions sont retrouvees par type/sous-type, aucun offset en dur.

## Compiler
    # placer ou lier le composant gamebuino :
    ln -s /chemin/vers/components/gamebuino components/gamebuino
    # (ou : export AKA_COMPONENTS=/chemin/vers/components)
    idf.py set-target esp32s3
    idf.py build
    # puis flasher build/aka_launcher.bin a l'adresse 0x340000

## Fichiers
| Fichier | Role |
|---|---|
| main/main.cpp | boucle principale, touches, installation |
| main/catalog.* | scan de la SD, lecture de meta.json |
| main/bmp.* | chargeur de screen.bmp vers le format natif du LCD |
| main/flasher.* | ecriture OTA de firmware.bin dans app0 |
| main/ui.* | dessin des ecrans (liste, fiche, progression, messages) |
| main/settings.* | derniere application (NVS), langue (settings.json + NVS) |
| main/i18n.* | textes fr/en, langues de la SD, fonction tr() |

## Flash direct dans app0 (VS Code)

Si le Launcher est flashé dans **app0** (comportement par défaut d'`idf.py flash`), il se détecte au démarrage,
se copie dans **app1**, sélectionne app1 puis redémarre dessus. app0 reste ainsi libre pour les jeux installés
depuis la carte SD. Aucune action requise ; la copie prend quelques secondes (écran de progression).

## Diagnostic d'une erreur d'installation
En cas d'echec, l'ecran affiche le texte de l'erreur puis des details techniques, par exemple :

    ota_end 0x1503
    OTA_VALIDATE_FAILED
    ecrit 933520/933520, SD courtes: 0
    flash=fichier (sha 42a419f1) OK

- **etape + code** : `ota_begin`, `ota_write`, `ota_end` ou `set_boot` avec le code `esp_err` (0x1503 = image invalide a la validation).
- **ecrit x/y** : octets effectivement envoyes ; **SD courtes** : lectures SD incompletes au milieu du fichier (signe d'un probleme de carte).
- **flash=fichier ... OK** : la partition relue (sans passer par le cache) est identique au fichier de la SD -> le probleme vient du fichier lui-meme
  (verifier avec `tools/aka_checkimg.py`). **flash!=fichier a/b** : les donnees ecrites different -> ecriture ou lecture SD defaillante.
Le meme detail est envoye sur la liaison serie (tag `flasher`), avec la partition courante (app1 attendue) et la cible (app0).
