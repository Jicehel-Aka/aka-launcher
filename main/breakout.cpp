#include "breakout.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "gamebuino.h"

extern gb_graphics gfx;

namespace launcher {

namespace {
constexpr int   kPadW = 40, kPadH = 4, kPadY = Breakout::kH - 10;
constexpr int   kBall = 4;
constexpr int   kBrickW = Breakout::kW / Breakout::kCols;   // 30
constexpr int   kBrickH = 8, kTop = 8;
constexpr float kPadSpeed = 4.0f;
}

void Breakout::reset() { m_lives = 3; m_score = 0; m_level = 1; newLevel(); }

void Breakout::newLevel()
{
    memset(m_brick, 1, sizeof m_brick);
    m_bricks = kRows * kCols;
    serve();
}

void Breakout::serve()
{
    m_px = kW / 2;
    m_stuck = true;
    m_bx = m_px; m_by = kPadY - kBall;
    m_vx = 0; m_vy = 0;
}

void Breakout::step(int dir, bool launch)
{
    m_px += dir * kPadSpeed;
    if (m_px < kPadW / 2) m_px = kPadW / 2;
    if (m_px > kW - kPadW / 2) m_px = kW - kPadW / 2;

    if (m_stuck) {
        m_bx = m_px; m_by = kPadY - kBall;
        if (launch || dir) {                           // un mouvement ou A lance la balle
            if (launch) {
                const float sp = 2.2f + 0.3f * (m_level - 1);
                m_vx = (dir < 0) ? -sp * 0.6f : sp * 0.6f;
                m_vy = -sp;
                m_stuck = false;
            }
        }
        return;
    }

    m_bx += m_vx; m_by += m_vy;
    if (m_bx < 0)            { m_bx = 0;            m_vx = -m_vx; }
    if (m_bx > kW - kBall)   { m_bx = kW - kBall;   m_vx = -m_vx; }
    if (m_by < 0)            { m_by = 0;            m_vy = -m_vy; }

    // Raquette : l'angle depend de l'endroit touche.
    if (m_vy > 0 && m_by + kBall >= kPadY && m_by + kBall <= kPadY + kPadH + 3 &&
        m_bx + kBall >= m_px - kPadW / 2 && m_bx <= m_px + kPadW / 2) {
        const float sp = std::sqrt(m_vx * m_vx + m_vy * m_vy);
        const float off = ((m_bx + kBall / 2) - m_px) / (kPadW / 2);     // -1..+1
        m_vx = off * sp * 0.8f;
        m_vy = -std::sqrt(sp * sp - m_vx * m_vx);
        m_by = kPadY - kBall;
    }

    // Briques.
    const int c = (int)((m_bx + kBall / 2) / kBrickW);
    const int r = (int)((m_by + kBall / 2 - kTop) / kBrickH);
    if (r >= 0 && r < kRows && c >= 0 && c < kCols && m_brick[r][c]) {
        m_brick[r][c] = false;
        --m_bricks; m_score += 10;
        // rebond : sur l'axe de plus faible penetration
        const float cx = c * kBrickW + kBrickW / 2.0f, cy = kTop + r * kBrickH + kBrickH / 2.0f;
        if (std::fabs(m_bx + kBall / 2 - cx) / kBrickW > std::fabs(m_by + kBall / 2 - cy) / kBrickH)
            m_vx = -m_vx;
        else
            m_vy = -m_vy;
        if (m_bricks == 0) { ++m_level; newLevel(); return; }
    }

    // Balle perdue.
    if (m_by > kH) {
        if (--m_lives <= 0) reset();
        else serve();
    }
}

void Breakout::draw(int x, int y, uint16_t bg, uint16_t frame, uint16_t paddle, uint16_t ball,
                    const uint16_t* rowColors, uint16_t text) const
{
    gfx.setColor(bg);
    gfx.fillRect(x, y, kW, kH);
    gfx.setColor(frame);
    gfx.drawRect(x - 1, y - 1, kW + 2, kH + 2);

    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kCols; ++c)
            if (m_brick[r][c]) {
                gfx.setColor(rowColors[r]);
                gfx.fillRect(x + c * kBrickW + 1, y + kTop + r * kBrickH + 1, kBrickW - 2, kBrickH - 2);
            }

    gfx.setColor(paddle);
    gfx.fillRect(x + (int)m_px - kPadW / 2, y + kPadY, kPadW, kPadH);
    gfx.setColor(ball);
    gfx.fillRect(x + (int)m_bx, y + (int)m_by, kBall, kBall);

    // Score et vies, en bas a gauche / a droite, dans l'aire de jeu.
    char s[24];
    snprintf(s, sizeof s, "%d", m_score);
    gfx.setColor(text);
    gfx.move_cursor((uint16_t)(x + 2), (uint16_t)(y + kH - 9)); gfx.print_str(s);
    snprintf(s, sizeof s, "x%d", m_lives);
    gfx.move_cursor((uint16_t)(x + kW - 2 - (int)strlen(s) * 8), (uint16_t)(y + kH - 9)); gfx.print_str(s);
}

}  // namespace launcher
