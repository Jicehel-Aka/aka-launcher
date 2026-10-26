// breakout.h - mini casse-brique affiche sous le message d'erreur (pas de carte SD, etc.).
#pragma once
#include <cstdint>

namespace launcher {

class Breakout {
public:
    static constexpr int kCols = 10, kRows = 4;
    static constexpr int kW = 304, kH = 132;          // aire de jeu (pixels)

    Breakout() { reset(); }
    void reset();
    // Avance d'un pas fixe (~16 ms). dir : -1 gauche, 0 rien, +1 droite ; launch : lancer la balle.
    void step(int dir, bool launch);
    // Dessine l'aire de jeu avec son coin haut-gauche en (x, y).
    void draw(int x, int y, uint16_t bg, uint16_t frame, uint16_t paddle, uint16_t ball,
              const uint16_t* rowColors, uint16_t text) const;

private:
    void newLevel();
    void serve();

    bool    m_brick[kRows][kCols];
    int     m_bricks = 0;
    float   m_px = 0;                                  // centre de la raquette
    float   m_bx = 0, m_by = 0, m_vx = 0, m_vy = 0;
    bool    m_stuck = true;
    int     m_lives = 3, m_score = 0, m_level = 1;
};

}  // namespace launcher
