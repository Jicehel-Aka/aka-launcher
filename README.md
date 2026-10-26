# Launcher Gamebuino AKA (code neuf)

Launcher en C++ pour ESP-IDF 5.x / ESP32-S3, au-dessus de la bibliotheque
`components/gamebuino`. Il vit dans la partition **loader (app/test, 0x10000)** ;
les applications sont installees dans la partition **game (app/factory, 0x100000)**.

## Fonctionnement
- Parcourt la racine de la carte SD : un dossier = une application si elle
  contient `firmware.bin` ; `meta.json` (title, description, author, version,
  date) et `screen.bpm` (BMP 16/24/32 bits, ~160x120) sont facultatifs.
- **[A]** installe l'application choisie dans **game (factory)** (effacement secteur
  par secteur pendant l'ecriture, puis controle d'integrite par `esp_image_verify`)
  et redemarre dessus.
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

## Partitions et demarrage

Table (`partitions.csv`) :

| Partition | Type / sous-type | Offset | Taille | Contenu |
|---|---|---|---|---|
| nvs | data / nvs | 0x9000 | 0x6000 | reglages |
| phy_init | data / phy | 0xF000 | 0x1000 | |
| loader | app / test | 0x10000 | 0xF0000 (960 Ko) | le Launcher |
| game | app / factory | 0x100000 | 0x700000 (7 Mo) | l'application installee |

Pas d'otadata ni d'`esp_ota_*` : le bootloader modifie
(`bootloader_components/main/bootloader_start.c`) choisit la partition selon la
**cause du reset** :

| Cause | Code S3 | Partition demarree |
|---|---|---|
| Mise sous tension | 0x01 | loader |
| Reset par watchdog RTC (`restartToLoader()`) | 0x10 | loader |
| Flash ou moniteur depuis VS Code (USB Serial/JTAG) | 0x15 | game |
| Reset logiciel (`restartToGame()` = `esp_restart`), panic, autres | — | game |

Replis du bootloader : si `game` est vide ou corrompu, il demarre `loader` ; si
`loader` est corrompu, il fait un reset logiciel et demarre donc `game`.

Une application qui plante en boucle : eteindre puis rallumer ramene au Launcher.

## Flash direct depuis VS Code

`idf.py flash` ecrit l'application dans la partition `factory` : le Launcher y
atterrit comme n'importe quel jeu. Au demarrage, il detecte qu'il tourne depuis
`factory` (`runningAsGame()`), se copie dans `test` (`installSelfToLoader()`, ecran
de progression), puis affiche « Eteignez la console » (`LAUNCHER_POWER_OFF`) : a la
mise sous tension suivante, le bootloader demarre `test`. Le redemarrage automatique
par watchdog RTC (`restartToLoader()`) ne fonctionne pas sur la console ; il n'est
plus utilise ici.

La copie reste en `factory` (une application ne peut pas ecrire dans sa propre
partition : `CONFIG_SPI_FLASH_DANGEROUS_WRITE_ABORTS`). Elle est sans effet : si
elle redemarre (reset logiciel, panic, [L1]+[R1]), `compareLoader()` constate que
`test` contient deja la meme image et demande directement d'eteindre, sans recopie.

## Mise a jour du Launcher depuis la carte SD

Meme mecanisme, sans PC : poser le nouveau Launcher dans un dossier de la carte
(`firmware.bin`, plus `meta.json` / `screen.bmp` si on veut), et le lancer depuis
le Launcher comme une application.

1. L'ancien Launcher (en `test`) ecrit le nouveau dans `factory` et redemarre (reset logiciel).
2. Le bootloader demarre `factory` : le nouveau Launcher se voit hors de sa place.
3. Il se recopie dans `test` (l'ancien n'est pas en cours d'execution, il peut etre ecrase).
4. Il demande d'eteindre la console : au rallumage, le nouveau Launcher tourne depuis `test`.

Robustesse :
- Coupure pendant l'etape 1 : `factory` est invalide, l'ancien Launcher en `test` est intact.
- Coupure pendant l'etape 3 : `test` est invalide, mais `factory` est toujours valide.
  Au rallumage, le bootloader echoue sur `test`, fait un reset logiciel et demarre
  `factory` : la copie reprend.
- Image trop grande pour `test` (> 960 Ko) : refusee avant tout effacement, l'ancien
  Launcher reste en place.

Limites :
- Le bootloader et la table de partitions ne se mettent a jour que par USB.
- Toute version peut etre installee, y compris une plus ancienne (pas de controle de version).

## Compiler
    # placer ou lier le composant gamebuino :
    ln -s /chemin/vers/components/gamebuino components/gamebuino
    # (ou : export AKA_COMPONENTS=/chemin/vers/components)
    idf.py set-target esp32s3
    idf.py build
    idf.py flash      # le Launcher se reinstalle seul en partition test au premier demarrage

## Fichiers
| Fichier | Role |
|---|---|
| main/main.cpp | boucle principale, touches, installation, auto-installation |
| main/catalog.* | scan de la SD, lecture de meta.json |
| main/bmp.* | chargeur de screen.bpm vers le format natif du LCD |
| main/flasher.* | ecriture de firmware.bin dans `factory`, auto-installation dans `test`, redemarrage vers le jeu ou le Launcher |
| main/ui.* | dessin des ecrans (liste, fiche, progression, messages, ecran d'erreur + casse-brique) |
| main/breakout.* | mini casse-brique de l'ecran « rien a lancer » |
| main/settings.* | derniere application (NVS), langue (settings.json + NVS) |
| main/i18n.* | textes fr/en, langues de la SD, fonction tr() |

## A faire
- Tester sur la console : retour par `restartToLoader()` (`rst:0x10` attendu).
- Deplacer `restartToLoader()` dans gb_lib pour le combo [HOME]+[MENU] des applications.
- Livrer `bootloader_components/` avec gb_lib (modele de projet), pour que chaque
  projet de jeu flashe le bon bootloader.
- Portage S31 : meme table, bootloader en 0x2000, reset Serial/JTAG = 0x17,
  watchdog RTC = `RESET_REASON_SYS_RWDT`.

## Disposition OTA (ancienne console)
Si la table ne contient pas `test` + `factory` mais `ota_0` + `ota_1`, le Launcher se comporte comme avant :
il tourne en app1 (OTA_1), installe dans app0 (OTA_0), selectionne app0 avec `esp_ota_set_boot_partition`
avant de redemarrer, et, flashe dans app0 par `idf.py flash`, se recopie dans app1 et redemarre directement
dessus (pas besoin d'eteindre). Le choix se fait dans `flasher.cpp` (`layout()`).

## Erreurs detaillees
`lastErrorDetail()` : etape, code `esp_err`, octets ecrits, lectures SD courtes, CRC32 du fichier et de la
flash relue (`flash=fichier` / `flash!=fichier`), disposition et partitions (`run=` / `cible=`). Affiche sous le
message d'erreur et envoye sur la liaison serie.

## Historique
- **2026-10-26 (casse-brique)** : sans carte SD lisible ou sans application, l'ecran affiche le message
  dans un bandeau rouge bien visible et un mini casse-brique en dessous (gauche/droite = raquette,
  [A] = lancer la balle). [D] relit la carte, [MENU] options, [C] aide : des que la carte est
  lue, la liste normale revient.
- **2026-10-26** : gestion des deux dispositions (OTA et loader/game) ; erreurs detaillees reintegrees.
- **2026-10-04 (correctif)** : boucle au premier demarrage (BOOT CODE 4 = panic).
  L'invalidation de la copie en `factory` ecrivait dans la partition en cours
  d'execution, ce que l'IDF interdit (`abort()`) ; la panic redemarrait `factory`,
  qui se reinstallait. Invalidation supprimee, ajout de `compareLoader()` : si `test`
  contient deja la meme image, pas de recopie, redemarrage direct vers `test`.
- **2026-10-04** : passage de app0/app1 (OTA) a `test` (loader, 0x10000) + `factory`
  (jeu, 0x100000) avec bootloader modifie selon la cause du reset. Plus d'`esp_ota_*`
  ni d'otadata. Auto-installation depuis `factory`, ce qui permet aussi la mise a
  jour du Launcher depuis la carte SD.
- Avant : Launcher en app1 (OTA_1, 0x340000), applications en app0 (OTA_0), copie
  app0 -> app1 au premier demarrage apres un flash VS Code.
