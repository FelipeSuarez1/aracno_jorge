// Canvas: framebuffer 320×240 RGB565 em memória (uint16_t nativo; a ordem
// de bytes do fio — ILI9341 espera byte alto primeiro — é tarefa do driver).
// Sem alocação, sem float, sem dependência de hardware. Compartilhado entre o
// firmware e tests/face.cpp; tools/face/render.py --compose replica
// exatamente esta lógica em Python para gerar tests/ref/*.ppm.
#pragma once
#include <stdint.h>

namespace aracno {
namespace face {

static const int CANVAS_W = 320;
static const int CANVAS_H = 240;
static const int FACE_H   = 160;  // rosto: y 0..159; balão: y 160..239

struct Rect {
    int16_t  x, y;
    uint16_t w, h;
};

// Sprite em .rodata: posição absoluta no canvas (x,y) com intensidade 100 e
// pixels RGB565 por linha. px == 0 → camada ausente nessa expressão.
struct Sprite {
    int16_t         x, y;
    uint16_t        w, h;
    const uint16_t* px;
};

// Glifo 8×16, 1 bit por pixel, MSB = coluna da esquerda (font16.h).
static const int GLYPH_W = 8;
static const int GLYPH_H = 16;

// Decodifica um code point UTF-8 a partir de *s; avança *s. Sequências
// inválidas rendem U+FFFD (um byte consumido).
uint32_t utf8_next(const char** s);

// Índice do glifo para o code point (ou do '?' se não houver).
int glyph_index(uint32_t cp);
const uint8_t* glyph_rows(int index);  // 16 bytes

class Canvas {
public:
    Canvas();
    void clear(uint16_t color);
    // Blit opaco, recortado nas bordas do canvas.
    void blit(const Sprite& s, int x, int y);
    // Blit com escala uniforme inteira (vizinho mais próximo), scale_pct em
    // 1..100, centrado na caixa original do sprite. 100 == blit(s, x, y).
    void blit_scaled(const Sprite& s, int x, int y, int scale_pct);
    // Uma linha de texto (sem quebra); glifos 8×16 com avanço 8 px.
    void text(int x, int y, const char* utf8, uint16_t color);
    // Retângulo preenchido (fill) ou só contorno de 1 px (outline).
    void rect(int x, int y, int w, int h, uint16_t color, bool fill);

    uint16_t at(int x, int y) const { return px[y * CANVAS_W + x]; }
    const uint16_t* data() const { return px; }

private:
    uint16_t px[CANVAS_W * CANVAS_H];
};

}  // namespace face
}  // namespace aracno
