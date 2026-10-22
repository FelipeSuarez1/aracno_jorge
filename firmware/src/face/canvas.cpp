// Ver canvas.h. Espelho em Python: tools/face/render.py (classe PyCanvas).
#include "face/canvas.h"
#include "face/font16.h"

namespace aracno {
namespace face {

uint32_t utf8_next(const char** s) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(*s);
    unsigned char c = p[0];
    int n;
    uint32_t cp;
    if (c < 0x80)       { n = 0; cp = c; }
    else if (c < 0xC0)  { *s += 1; return 0xFFFD; }  // continuação solta
    else if (c < 0xE0)  { n = 1; cp = c & 0x1F; }
    else if (c < 0xF0)  { n = 2; cp = c & 0x0F; }
    else if (c < 0xF8)  { n = 3; cp = c & 0x07; }
    else                { *s += 1; return 0xFFFD; }
    for (int i = 1; i <= n; ++i) {
        if ((p[i] & 0xC0) != 0x80) { *s += 1; return 0xFFFD; }
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *s += n + 1;
    return cp;
}

int glyph_index(uint32_t cp) {
    if (cp >= 32 && cp <= 126) return static_cast<int>(cp - 32);
    for (int i = FONT16_ASCII_N; i < FONT16_N; ++i)
        if (FONT16_CP[i] == cp) return i;
    return '?' - 32;
}

const uint8_t* glyph_rows(int index) { return FONT16_ROWS[index]; }

Canvas::Canvas() { clear(0); }

void Canvas::clear(uint16_t color) {
    for (int i = 0; i < CANVAS_W * CANVAS_H; ++i) px[i] = color;
}

void Canvas::blit(const Sprite& s, int x, int y) {
    if (!s.px) return;
    for (int r = 0; r < s.h; ++r) {
        int dy = y + r;
        if (dy < 0 || dy >= CANVAS_H) continue;
        const uint16_t* src = s.px + r * s.w;
        uint16_t* dst = px + dy * CANVAS_W;
        for (int c = 0; c < s.w; ++c) {
            int dx = x + c;
            if (dx < 0 || dx >= CANVAS_W) continue;
            dst[dx] = src[c];
        }
    }
}

void Canvas::blit_scaled(const Sprite& s, int x, int y, int scale_pct) {
    if (!s.px) return;
    if (scale_pct >= 100) { blit(s, x, y); return; }
    if (scale_pct < 1) scale_pct = 1;
    int dw = (s.w * scale_pct) / 100;
    int dh = (s.h * scale_pct) / 100;
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    int x0 = x + (s.w - dw) / 2;
    int y0 = y + (s.h - dh) / 2;
    for (int r = 0; r < dh; ++r) {
        int dy = y0 + r;
        if (dy < 0 || dy >= CANVAS_H) continue;
        const uint16_t* src = s.px + ((r * s.h) / dh) * s.w;
        uint16_t* dst = px + dy * CANVAS_W;
        for (int c = 0; c < dw; ++c) {
            int dx = x0 + c;
            if (dx < 0 || dx >= CANVAS_W) continue;
            dst[dx] = src[(c * s.w) / dw];
        }
    }
}

void Canvas::text(int x, int y, const char* utf8, uint16_t color) {
    const char* s = utf8;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        const uint8_t* rows = glyph_rows(glyph_index(cp));
        for (int r = 0; r < GLYPH_H; ++r) {
            int dy = y + r;
            if (dy < 0 || dy >= CANVAS_H) continue;
            uint8_t bits = rows[r];
            for (int c = 0; c < GLYPH_W; ++c) {
                if (!(bits & (0x80 >> c))) continue;
                int dx = x + c;
                if (dx < 0 || dx >= CANVAS_W) continue;
                px[dy * CANVAS_W + dx] = color;
            }
        }
        x += GLYPH_W;
    }
}

void Canvas::rect(int x, int y, int w, int h, uint16_t color, bool fill) {
    if (w <= 0 || h <= 0) return;
    for (int r = 0; r < h; ++r) {
        int dy = y + r;
        if (dy < 0 || dy >= CANVAS_H) continue;
        bool edge_row = (r == 0 || r == h - 1);
        for (int c = 0; c < w; ++c) {
            int dx = x + c;
            if (dx < 0 || dx >= CANVAS_W) continue;
            if (fill || edge_row || c == 0 || c == w - 1)
                px[dy * CANVAS_W + dx] = color;
        }
    }
}

}  // namespace face
}  // namespace aracno
