#include "medula/model.h"
#include <string.h>

namespace aracno { namespace medula {

Params default_params() {
    Params p;
    p.fg_delta = 8;
    p.min_count = 40;
    p.reflex_near = 38;
    p.reflex_drop = 4;
    p.hold_ms = 1500;
    p.pan_gain = 66;
    p.tilt_gain = 68;
    p.slew_us = 26;
    p.reflex_en = true;
    return p;
}

// Deslocamento aritmético de 4 bits (floor), como o `>>>` do RTL.
static int32_t asr4(int32_t v) { return v >= 0 ? v >> 4 : ~((~v) >> 4); }

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

// step(b, c) = b + (c > b) - (c < b)
static uint8_t step(uint8_t b, uint8_t c) {
    return static_cast<uint8_t>(b + (c > b ? 1 : 0) - (c < b ? 1 : 0));
}

Model::Model()
    : frames_(0), prev_target_(false), prev_min_(0), pan_t_(CENTER_US), tilt_t_(CENTER_US) {
    memset(bg_, 0, sizeof bg_);
}

void Model::set_frames(uint32_t fi) { frames_ = fi; }

void Model::process(const uint8_t* depth8, bool bg_reset, const Params& p, Result* out) {
    const uint32_t fi = frames_;
    bool fg_row[W];                      // fg_raw da linha anterior (vizinho de cima)
    memset(fg_row, 0, sizeof fg_row);

    uint32_t count = 0, sum_x = 0, sum_y = 0, sum_d = 0;
    uint8_t min_d = 255, bx0 = 255, by0 = 255, bx1 = 0, by1 = 0;

    for (int y = 0; y < H; ++y) {
        bool fg_left = false;            // fora da imagem = 0
        for (int x = 0; x < W; ++x) {
            const int pi = y * W + x;
            const uint8_t cur = depth8[pi];
            const uint8_t bg = bg_[pi];

            const bool fg_raw = cur != 0 && bg != 0 && bg > cur && (bg - cur) >= p.fg_delta;
            const bool fg_up = y > 0 && fg_row[x];
            const bool fg_clean = fg_raw && fg_left && fg_up;

            uint8_t nbg;
            if (bg_reset)        nbg = cur;
            else if (bg == 0)    nbg = cur;
            else if (cur == 0)   nbg = bg;
            else if (fg_raw)     nbg = ((fi & 15) == 0) ? step(bg, cur) : bg;
            else                 nbg = step(bg, cur);
            bg_[pi] = nbg;

            if (fg_clean) {
                ++count;
                sum_x += static_cast<uint32_t>(x);
                sum_y += static_cast<uint32_t>(y);
                sum_d += cur;
                if (cur < min_d) min_d = cur;
                if (x < bx0) bx0 = static_cast<uint8_t>(x);
                if (y < by0) by0 = static_cast<uint8_t>(y);
                if (x > bx1) bx1 = static_cast<uint8_t>(x);
                if (y > by1) by1 = static_cast<uint8_t>(y);
            }
            fg_row[x] = fg_raw;
            fg_left = fg_raw;
        }
    }

    Result r;
    memset(&r, 0, sizeof r);
    r.count = static_cast<uint16_t>(count);
    r.target = count >= p.min_count;
    // count = 0 com MIN_COUNT = 0: divisão por zero não definida no contrato; o modelo informa tudo 0.
    if (r.target && count != 0) {
        r.cx = static_cast<uint8_t>(sum_x / count);
        r.cy = static_cast<uint8_t>(sum_y / count);
        r.mean_depth = static_cast<uint8_t>(sum_d / count);
        r.min_depth = min_d;
        r.x0 = bx0; r.y0 = by0; r.x1 = bx1; r.y1 = by1;
    }

    if (r.target) {
        pan_t_ = static_cast<uint16_t>(clampi(CENTER_US + asr4((static_cast<int32_t>(r.cx) - 80) * p.pan_gain),
                                              PAN_MIN_US, PAN_MAX_US));
        tilt_t_ = static_cast<uint16_t>(clampi(CENTER_US - asr4((static_cast<int32_t>(r.cy) - 60) * p.tilt_gain),
                                               TILT_MIN_US, TILT_MAX_US));
    }

    // Reflexo: a soma é feita em int (equivale aos 9 bits do RTL), nunca subtração sem sinal.
    r.reflex = p.reflex_en && r.target && prev_target_
            && static_cast<int>(prev_min_) >= static_cast<int>(r.min_depth) + static_cast<int>(p.reflex_drop)
            && r.min_depth <= p.reflex_near;
    prev_target_ = r.target;
    prev_min_ = r.min_depth;

    r.pan_target_us = pan_t_;
    r.tilt_target_us = r.reflex ? static_cast<uint16_t>(TILT_MAX_US) : tilt_t_;
    r.latency_cycles = 0;

    ++frames_;
    *out = r;
}

}}  // namespace aracno::medula
