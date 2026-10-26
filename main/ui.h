// ui.h - dessin des ecrans du Launcher (320x240, police 8x8 ASCII).
#pragma once
#include <cstddef>
#include <cstdint>
#include "catalog.h"
#include "breakout.h"

namespace launcher {

class Ui {
public:
    Ui();

    // Apercu de l'application affichee ; appeler quand la selection change.
    void loadPreview(const Catalog& cat, int index);

    // Ecran principal : liste a gauche, apercu + fiche a droite.
    void drawBrowser(const Catalog& cat, int selected, int top, bool sdOk);

    // Ecran "rien a lancer" (SD absente / illisible / aucune appli) : message bien visible + casse-brique.
    void drawErrorGame(const Breakout& game, bool sdOk);

    // Ecran d'information (message centre, plusieurs lignes separees par '\n').
    void drawMessage(const char* title, const char* text, bool error);

    // Ecran des options (langue) ; langIndex = indice dans la liste i18n.
    void drawOptions(int langIndex);

    // Aide : page `page` (0..pages-1).
    void drawHelp(int page, int pages);

    // Barre de progression d'installation.
    void drawProgress(const char* appTitle, size_t done, size_t total);

    static constexpr int kVisibleRows = 16;

private:
    void text(int x, int y, const char* s, uint16_t color);
    void wrapped(int x, int y, int cols, int maxLines, const char* s, uint16_t color);

    uint16_t m_preview[kMaxPreviewW * kMaxPreviewH];
    int      m_previewW = 0, m_previewH = 0;
    bool     m_hasPreview = false;
};

}  // namespace launcher
