#include "ui.h"

#include <cstdio>
#include <cstring>

#include "bmp.h"
#include "gb_graphics.h"
#include "i18n.h"

extern gb_graphics gfx;

namespace launcher {

namespace {

struct Palette {
    uint16_t bg, panel, bar, text, dim, accent, selBg, selText, ok, err, frame;
};

Palette makePalette()
{
    Palette p;
    p.bg      = gfx.makeColor(14, 16, 28);
    p.panel   = gfx.makeColor(24, 28, 46);
    p.bar     = gfx.makeColor(36, 60, 120);
    p.text    = gfx.makeColor(220, 224, 236);
    p.dim     = gfx.makeColor(130, 138, 160);
    p.accent  = gfx.makeColor(250, 200, 60);
    p.selBg   = gfx.makeColor(60, 110, 200);
    p.selText = gfx.makeColor(255, 255, 255);
    p.ok      = gfx.makeColor(80, 210, 110);
    p.err     = gfx.makeColor(240, 90, 90);
    p.frame   = gfx.makeColor(70, 78, 110);
    return p;
}

constexpr int kListX = 4, kListW = 150, kRowH = 12, kListY = 24;
constexpr int kRightX = 158;                 // panneau de droite : 160 px
constexpr int kPreviewY = 24;
constexpr int kCols = 20;                    // 160 px / 8 px

}  // namespace

Ui::Ui() { memset(m_preview, 0, sizeof m_preview); }

void Ui::text(int x, int y, const char* s, uint16_t color)
{
    gfx.setColor(color);
    gfx.move_cursor((uint16_t)x, (uint16_t)y);
    gfx.print_str(s);
}

void Ui::wrapped(int x, int y, int cols, int maxLines, const char* s, uint16_t color)
{
    char line[64];
    const size_t len = strlen(s);
    size_t pos = 0;
    for (int l = 0; l < maxLines && pos < len; ++l) {
        size_t take = len - pos < (size_t)cols ? len - pos : (size_t)cols;
        if (pos + take < len) {                          // coupe sur un espace si possible
            size_t k = take;
            while (k > 0 && s[pos + k] != ' ') --k;
            if (k > 0) take = k;
        }
        memcpy(line, s + pos, take);
        line[take] = '\0';
        text(x, y + l * 10, line, color);
        pos += take;
        while (pos < len && s[pos] == ' ') ++pos;
    }
}

void Ui::loadPreview(const Catalog& cat, int index)
{
    m_hasPreview = false;
    if (index < 0 || (size_t)index >= cat.count() || !cat.at(index).hasPreview) return;
    char path[320];
    cat.path(index, kScreenName, path, sizeof path);
    m_hasPreview = loadBmp(path, m_preview, kMaxPreviewW, kMaxPreviewH, &m_previewW, &m_previewH);
    if (!m_hasPreview) {
        cat.path(index, kScreenNameAlt, path, sizeof path);
        m_hasPreview = loadBmp(path, m_preview, kMaxPreviewW, kMaxPreviewH, &m_previewW, &m_previewH);
    }
}

void Ui::drawBrowser(const Catalog& cat, int selected, int top, bool sdOk)
{
    static const Palette c = makePalette();

    gfx.clear(c.bg);

    // Bandeau de titre
    gfx.setColor(c.bar);
    gfx.fillRect(0, 0, 320, 18);
    text(8, 5, tr("LAUNCHER_TITLE"), c.text);
    char count[16];
    snprintf(count, sizeof count, tr("LAUNCHER_APPS_COUNT"), (unsigned)cat.count());
    text(320 - 8 - (int)strlen(count) * 8, 5, count, c.accent);

    // Liste
    gfx.setColor(c.panel);
    gfx.fillRect(kListX - 2, kListY - 3, kListW + 2, kVisibleRows * kRowH + 4);

    if (!sdOk) {
        text(kListX + 4, kListY + 8, tr("LAUNCHER_SD_MISSING1"), c.err);
        text(kListX + 4, kListY + 22, tr("LAUNCHER_SD_MISSING2"), c.err);
        text(kListX + 4, kListY + 42, tr("LAUNCHER_SD_RETRY"), c.dim);
    } else if (cat.count() == 0) {
        text(kListX + 4, kListY + 8, tr("LAUNCHER_NO_APP1"), c.dim);
        text(kListX + 4, kListY + 24, tr("LAUNCHER_NO_APP2"), c.dim);
        text(kListX + 4, kListY + 34, tr("LAUNCHER_NO_APP3"), c.dim);
    }

    for (int row = 0; row < kVisibleRows; ++row) {
        const int i = top + row;
        if ((size_t)i >= cat.count()) break;
        const int y = kListY + row * kRowH;
        const bool sel = (i == selected);
        if (sel) {
            gfx.setColor(c.selBg);
            gfx.fillRect(kListX - 2, y - 1, kListW + 2, kRowH);
        }
        char label[24];
        const char* t = cat.at(i).title;
        if (strlen(t) > 18) { snprintf(label, sizeof label, "%.16s..", t); }
        else                { snprintf(label, sizeof label, "%.18s", t); }
        text(kListX + 2, y + 1, label, sel ? c.selText : c.text);
    }
    // Indicateurs de defilement
    if (top > 0) text(kListX + kListW - 8, kListY - 2, "^", c.accent);
    if ((size_t)(top + kVisibleRows) < cat.count())
        text(kListX + kListW - 8, kListY + kVisibleRows * kRowH - 9, "v", c.accent);

    // Fiche de l'application selectionnee
    if (sdOk && selected >= 0 && (size_t)selected < cat.count()) {
        const AppEntry& a = cat.at(selected);

        gfx.setColor(c.frame);
        gfx.drawRect(kRightX - 1, kPreviewY - 1, kMaxPreviewW + 2, kMaxPreviewH + 2);
        if (m_hasPreview) {
            gfx.drawImage(kRightX + (kMaxPreviewW - m_previewW) / 2,
                          kPreviewY + (kMaxPreviewH - m_previewH) / 2,
                          m_preview, (uint16_t)m_previewW, (uint16_t)m_previewH);
        } else {
            { const char* np = tr("LAUNCHER_NO_PREVIEW");
            text(kRightX + (kMaxPreviewW - (int)strlen(np) * 8) / 2, kPreviewY + 56, np, c.dim); }
        }

        int y = kPreviewY + kMaxPreviewH + 6;
        char line[64];
        snprintf(line, sizeof line, "%.20s", a.title);
        text(kRightX, y, line, c.accent);
        y += 10;
        if (a.author[0]) {
            snprintf(line, sizeof line, "%.4s %.14s", tr("LAUNCHER_BY"), a.author);
            text(kRightX, y, line, c.text);
            y += 10;
        }
        if (a.version[0] || a.date[0]) {
            snprintf(line, sizeof line, "v%.12s %.12s", a.version, a.date);
            text(kRightX, y, line, c.dim);
            y += 10;
        }
        wrapped(kRightX, y + 2, kCols, (226 - (y + 2)) / 10, a.description, c.text);
    }

    // Pied de page
    gfx.setColor(c.bar);
    gfx.fillRect(0, 228, 320, 12);
    text(4, 231, tr("LAUNCHER_FOOTER"), c.text);
}

void Ui::drawErrorGame(const Breakout& game, bool sdOk)
{
    static const Palette c = makePalette();
    gfx.clear(c.bg);

    gfx.setColor(c.bar);
    gfx.fillRect(0, 0, 320, 18);
    text(8, 5, tr("LAUNCHER_TITLE"), c.text);

    // Message : grand bandeau plein, texte blanc sur fond rouge, centre.
    gfx.setColor(c.err);
    gfx.fillRect(8, 24, 304, 48);
    const char* l1 = sdOk ? tr("LAUNCHER_NO_APP1") : tr("LAUNCHER_SD_MISSING1");
    const char* l2 = sdOk ? tr("LAUNCHER_NO_APP3") : tr("LAUNCHER_SD_MISSING2");
    text(160 - (int)strlen(l1) * 4, 30, l1, c.selText);
    text(160 - (int)strlen(l2) * 4, 44, l2, c.selText);
    const char* rt = tr("LAUNCHER_SD_RETRY");
    text(160 - (int)strlen(rt) * 4, 58, rt, c.selText);

    const uint16_t rows[Breakout::kRows] = { c.err, c.accent, c.ok, c.selBg };
    game.draw(8, 90, c.panel, c.frame, c.selText, c.accent, rows, c.dim);

    const char* hint = tr("LAUNCHER_GAME_HINT");
    text(160 - (int)strlen(hint) * 4, 78, hint, c.dim);

    gfx.setColor(c.bar);
    gfx.fillRect(0, 228, 320, 12);
    text(4, 231, tr("LAUNCHER_FOOTER"), c.text);
}

void Ui::drawMessage(const char* title, const char* msg, bool error)
{
    static const Palette c = makePalette();
    gfx.clear(c.bg);
    gfx.setColor(error ? c.err : c.bar);
    gfx.fillRect(0, 0, 320, 18);
    text(8, 5, title, c.selText);

    int y = 60;
    char line[48];
    const char* p = msg;
    while (*p) {
        size_t n = strcspn(p, "\n");
        if (n > 38) n = 38;                       // 320 px / 8 px par caractere : on coupe au lieu de deborder
        memcpy(line, p, n);
        line[n] = '\0';
        text(160 - (int)n * 4, y, line, error ? c.err : c.text);
        y += 14;
        p += n;
        if (*p == '\n') ++p;
    }
    { const char* pk = tr("LAUNCHER_PRESS_KEY");
      text(160 - (int)strlen(pk) * 4, 220, pk, c.dim); }
}

void Ui::drawProgress(const char* appTitle, size_t done, size_t total)
{
    static const Palette c = makePalette();
    gfx.clear(c.bg);
    gfx.setColor(c.bar);
    gfx.fillRect(0, 0, 320, 18);
    text(8, 5, tr("LAUNCHER_INSTALLING"), c.selText);

    char line[48];
    snprintf(line, sizeof line, "%.36s", appTitle);
    text(160 - (int)strlen(line) * 4, 80, line, c.accent);

    const int bx = 40, by = 110, bw = 240, bh = 16;
    gfx.setColor(c.panel);
    gfx.fillRect(bx, by, bw, bh);
    const int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
    gfx.setColor(c.ok);
    gfx.fillRect(bx, by, bw * pct / 100, bh);
    gfx.setColor(c.frame);
    gfx.drawRect(bx - 1, by - 1, bw + 2, bh + 2);

    snprintf(line, sizeof line, "%d %%  (%u / %u Ko)", pct, (unsigned)(done / 1024), (unsigned)(total / 1024));
    text(160 - (int)strlen(line) * 4, 140, line, c.text);
    { const char* kp = tr("LAUNCHER_KEEP_POWER");
      text(160 - (int)strlen(kp) * 4, 190, kp, c.dim); }
}

void Ui::drawOptions(int langIndex)
{
    static const Palette c = makePalette();
    gfx.clear(c.bg);
    gfx.setColor(c.bar);
    gfx.fillRect(0, 0, 320, 18);
    text(8, 5, tr("LAUNCHER_OPT_TITLE"), c.text);

    // Ligne "Langue" : < Francais >
    gfx.setColor(c.selBg);
    gfx.fillRect(16, 40, 288, 20);
    text(24, 46, tr("LAUNCHER_OPT_LANG"), c.selText);
    const size_t nLang = languageCount();
    if (nLang > 0 && langIndex >= 0 && (size_t)langIndex < nLang) {
        char val[40];
        snprintf(val, sizeof val, "< %s >", languageAt(langIndex).name);
        text(296 - (int)strlen(val) * 8, 46, val, c.accent);
    }

    // Liste des langues disponibles (la courante est en surbrillance)
    int y = 80;
    for (size_t i = 0; i < nLang && y < 200; ++i, y += 12) {
        const bool cur = ((int)i == langIndex);
        char line[40];
        snprintf(line, sizeof line, "%c %s (%s)", cur ? '>' : ' ', languageAt(i).name, languageAt(i).code);
        text(40, y, line, cur ? c.accent : c.dim);
    }

    text(16, 204, tr("LAUNCHER_OPT_HELP1"), c.dim);
    text(16, 214, tr("LAUNCHER_OPT_HELP2"), c.dim);
    text(16, 224, tr("LAUNCHER_OPT_HELP3"), c.dim);
}

void Ui::drawHelp(int page, int pages)
{
    static const Palette c = makePalette();
    gfx.clear(c.bg);
    gfx.setColor(c.bar);
    gfx.fillRect(0, 0, 320, 18);
    text(8, 5, tr("LAUNCHER_HELP_TITLE"), c.text);
    char pg[16];
    snprintf(pg, sizeof pg, "%d/%d", page + 1, pages);
    text(320 - 8 - (int)strlen(pg) * 8, 5, pg, c.accent);

    char key[48];
    snprintf(key, sizeof key, "LAUNCHER_HELP%d_T", page + 1);
    text(8, 28, tr(key), c.accent);
    gfx.setColor(c.frame);
    gfx.drawFastHLine(8, 40, 304);

    snprintf(key, sizeof key, "LAUNCHER_HELP%d", page + 1);
    const char* body = tr(key);
    int y = 48;
    char line[48];
    for (const char* p = body; *p && y < 214; y += 11) {
        size_t n = strcspn(p, "\n");
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        text(8, y, line, c.text);
        p += n;
        if (*p == '\n') ++p;
    }
    gfx.setColor(c.bar);
    gfx.fillRect(0, 228, 320, 12);
    text(4, 231, tr("LAUNCHER_HELP_NAV"), c.text);
}

}  // namespace launcher
