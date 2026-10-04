// bmp.h - chargeur BMP minimal (screen.bmp) vers le format natif du LCD AKA.
#pragma once
#include <cstdint>

namespace launcher {

// Charge un BMP 16 bits (RGB565 ou RGB555), 24 bits ou 32 bits, non compresse.
// Les pixels ecrits dans `out` sont au format natif du framebuffer AKA
// (voir lcd_color_rgb : rouge dans les bits faibles). `out` doit pouvoir
// contenir maxW*maxH pixels. Retourne false si le fichier est invalide ou plus
// grand que maxW x maxH.
bool loadBmp(const char* path, uint16_t* out, int maxW, int maxH, int* w, int* h);

}  // namespace launcher
