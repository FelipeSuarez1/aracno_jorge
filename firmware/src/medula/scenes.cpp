#include "medula/scenes.h"
#include "medula/model.h"
#include <string.h>

namespace aracno { namespace medula {

static void fill(uint8_t* out, uint8_t v) { memset(out, v, N); }

static void rect(uint8_t* out, int x0, int y0, int x1, int y1, uint8_t v) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) out[y * W + x] = v;
}

// Cena 1 com o retângulo em `rect_v`.
static void with_rect(uint8_t* out, uint8_t rect_v) {
    fill(out, 150);
    rect(out, 60, 40, 79, 69, rect_v);
    for (int k = 0; k < 20; ++k) out[100 * W + 10 + 7 * k] = 50;
}

void make_scene(int id, uint8_t* out) {
    switch (id) {
    case 0: fill(out, 150); break;
    case 1: with_rect(out, 50); break;
    case 2: with_rect(out, 40); break;
    case 3: with_rect(out, 34); break;
    case 4: with_rect(out, 36); break;
    case 5:
        fill(out, 150);
        rect(out, 0, 0, 9, H - 1, 0);
        break;
    case 6:
        with_rect(out, 50);
        rect(out, 0, 0, 9, H - 1, 100);
        break;
    case 7:
        fill(out, 150);
        rect(out, 100, 20, 104, 24, 50);
        break;
    default: fill(out, 0); break;
    }
}

}}  // namespace aracno::medula
