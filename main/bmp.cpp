#include "bmp.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace launcher {

namespace {

inline uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t u32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

// r,g,b 8 bits -> pixel natif AKA
inline uint16_t native(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11));
}

}  // namespace

bool loadBmp(const char* path, uint16_t* out, int maxW, int maxH, int* w, int* h)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint8_t hdr[54];
    bool ok = fread(hdr, 1, sizeof hdr, f) == sizeof hdr && hdr[0] == 'B' && hdr[1] == 'M';
    if (!ok) { fclose(f); return false; }

    const uint32_t dataOff  = u32(hdr + 10);
    const uint32_t dibSize  = u32(hdr + 14);
    int32_t        width    = (int32_t)u32(hdr + 18);
    int32_t        height   = (int32_t)u32(hdr + 22);
    const uint16_t bpp      = u16(hdr + 28);
    const uint32_t compress = u32(hdr + 30);
    const bool     topDown  = height < 0;
    if (topDown) height = -height;

    if (dibSize < 40 || width <= 0 || height <= 0 || width > maxW || height > maxH ||
        (compress != 0 && compress != 3) || (bpp != 16 && bpp != 24 && bpp != 32)) {
        fclose(f);
        return false;
    }

    // Masques pour les BMP 16 bits en BI_BITFIELDS (565 par defaut ; 555 sinon).
    bool is565 = true;
    if (bpp == 16) {
        if (compress == 3) {
            uint8_t m[12];
            fseek(f, 14 + dibSize, SEEK_SET);
            if (fread(m, 1, 12, f) != 12) { fclose(f); return false; }
            is565 = (u32(m + 4) == 0x07E0);
        } else {
            is565 = false;                        // BI_RGB 16 bits = 555
        }
    }

    const size_t rowBytes = (((size_t)width * bpp + 31) / 32) * 4;
    uint8_t* row = (uint8_t*)malloc(rowBytes);
    if (!row) { fclose(f); return false; }

    for (int y = 0; y < height && ok; ++y) {
        const int dstY = topDown ? y : (height - 1 - y);
        fseek(f, (long)(dataOff + (size_t)y * rowBytes), SEEK_SET);
        if (fread(row, 1, rowBytes, f) != rowBytes) { ok = false; break; }

        uint16_t* dst = out + (size_t)dstY * width;
        for (int x = 0; x < width; ++x) {
            uint8_t r, g, b;
            if (bpp == 16) {
                const uint16_t v = u16(row + x * 2);
                if (is565) { r = (v >> 11) << 3; g = ((v >> 5) & 0x3F) << 2; b = (v & 0x1F) << 3; }
                else       { r = ((v >> 10) & 0x1F) << 3; g = ((v >> 5) & 0x1F) << 3; b = (v & 0x1F) << 3; }
            } else {
                const uint8_t* p = row + x * (bpp / 8);
                b = p[0]; g = p[1]; r = p[2];
            }
            dst[x] = native(r, g, b);
        }
    }
    free(row);
    fclose(f);
    if (ok) { *w = width; *h = height; }
    return ok;
}

}  // namespace launcher
