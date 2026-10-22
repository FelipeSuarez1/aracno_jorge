// Ver face.h. Espelho em Python: tools/face/render.py (compose()).
#include "face/face.h"
#include "face/sprites.h"

namespace aracno {
namespace face {

// ---- quebra de linha ------------------------------------------------------

namespace {

int cp_len(const char* s, int nbytes) {
    int n = 0;
    const char* end = s + nbytes;
    while (s < end) { utf8_next(&s); ++n; }
    return n;
}

struct Wrapper {
    char (*out)[BALLOON_LINE_BYTES];
    int  n;      // linhas fechadas
    int  cols;   // colunas na linha corrente
    int  bytes;  // bytes na linha corrente

    bool full() const { return n >= BALLOON_LINES; }
    void flush() {
        if (full()) return;
        out[n][bytes] = 0;
        ++n;
        cols = 0;
        bytes = 0;
    }
    void append(const char* s, int nbytes, int ncols) {
        if (full()) return;
        for (int i = 0; i < nbytes; ++i) out[n][bytes + i] = s[i];
        bytes += nbytes;
        cols += ncols;
    }
    void word(const char* w, int nbytes) {
        int ncols = cp_len(w, nbytes);
        if (ncols > BALLOON_COLS) {
            if (cols > 0) flush();
            // corta em pedaços de BALLOON_COLS code points
            const char* p = w;
            const char* end = w + nbytes;
            while (p < end && !full()) {
                const char* q = p;
                int c = 0;
                while (q < end && c < BALLOON_COLS) { utf8_next(&q); ++c; }
                append(p, static_cast<int>(q - p), c);
                if (q < end) flush();
                p = q;
            }
            return;
        }
        if (cols == 0) {
            append(w, nbytes, ncols);
        } else if (cols + 1 + ncols <= BALLOON_COLS) {
            append(" ", 1, 1);
            append(w, nbytes, ncols);
        } else {
            flush();
            append(w, nbytes, ncols);
        }
    }
};

}  // namespace

int wrap_lines(const char* utf8, char out[BALLOON_LINES][BALLOON_LINE_BYTES]) {
    Wrapper w = { out, 0, 0, 0 };
    const char* p = utf8;
    while (*p && !w.full()) {
        while (*p == ' ') ++p;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ') ++p;
        w.word(start, static_cast<int>(p - start));
    }
    if (w.cols > 0 && !w.full()) w.flush();
    return w.n;
}

// ---- Face -----------------------------------------------------------------

Face::Face()
    : expr_(NEUTRO), intensity_(100), blink_(false), started_(false),
      next_blink_(0), blink_end_(0), say_until_(0), say_pending_(false),
      rng_(BLINK_SEED), n_lines_(0), has_dirty_(false) {
    dirty_.x = 0; dirty_.y = 0; dirty_.w = 0; dirty_.h = 0;
    for (int i = 0; i < BALLOON_LINES; ++i) lines_[i][0] = 0;
    draw_face();
    draw_balloon();
    Rect all = { 0, 0, CANVAS_W, CANVAS_H };
    mark(all);
}

uint32_t Face::rand32() {
    uint32_t x = rng_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_ = x;
    return x;
}

uint32_t Face::blink_gap() {
    return BLINK_MIN_GAP_MS + rand32() % (BLINK_MAX_GAP_MS - BLINK_MIN_GAP_MS + 1);
}

void Face::mark(const Rect& r) {
    if (r.w == 0 || r.h == 0) return;
    if (!has_dirty_) { dirty_ = r; has_dirty_ = true; return; }
    int x0 = dirty_.x < r.x ? dirty_.x : r.x;
    int y0 = dirty_.y < r.y ? dirty_.y : r.y;
    int x1a = dirty_.x + dirty_.w, x1b = r.x + r.w;
    int y1a = dirty_.y + dirty_.h, y1b = r.y + r.h;
    int x1 = x1a > x1b ? x1a : x1b;
    int y1 = y1a > y1b ? y1a : y1b;
    dirty_.x = static_cast<int16_t>(x0);
    dirty_.y = static_cast<int16_t>(y0);
    dirty_.w = static_cast<uint16_t>(x1 - x0);
    dirty_.h = static_cast<uint16_t>(y1 - y0);
}

bool Face::dirty(Rect* out) const {
    if (!has_dirty_) return false;
    if (out) *out = dirty_;
    return true;
}

void Face::flushed() { has_dirty_ = false; }

void Face::draw_face() {
    canvas_.rect(0, 0, CANVAS_W, FACE_H, 0x0000, true);
    const int frame = blink_ ? 1 : 0;
    const int scale = intensity_scale(intensity_);
    for (int l = 0; l < N_LAYERS; ++l) {
        const Sprite& s = SPRITES[expr_][frame][l];
        if (!s.px) continue;
        if (l == L_BROW_L || l == L_BROW_R)
            canvas_.blit(s, s.x, s.y);
        else
            canvas_.blit_scaled(s, s.x, s.y, scale);
    }
}

void Face::draw_balloon() {
    canvas_.rect(0, BALLOON_Y, CANVAS_W, BALLOON_H, 0x0000, true);
    if (n_lines_ == 0) return;
    const uint16_t color = EXPR_COLOR[expr_];
    canvas_.rect(0, BALLOON_Y, CANVAS_W, BALLOON_H, color, false);
    for (int i = 0; i < n_lines_; ++i)
        canvas_.text(BALLOON_TEXT_X, BALLOON_TEXT_Y + i * BALLOON_LINE_PX,
                     lines_[i], color);
}

void Face::set(Expr e, uint8_t intensity) {
    if (e < 0 || e >= N_EXPR) e = NEUTRO;
    expr_ = e;
    intensity_ = intensity > 100 ? 100 : intensity;
    draw_face();
    draw_balloon();  // cor do balão segue o humor
    Rect all = { 0, 0, CANVAS_W, CANVAS_H };
    mark(all);
}

void Face::say(const char* utf8, uint32_t ms) {
    n_lines_ = wrap_lines(utf8, lines_);
    say_pending_ = (ms != 0);
    say_until_ = ms;  // relativo até o próximo tick (ver tick)
    draw_balloon();
    Rect r = { 0, BALLOON_Y, CANVAS_W, BALLOON_H };
    mark(r);
}

void Face::tick(uint32_t now_ms) {
    if (!started_) {
        started_ = true;
        next_blink_ = now_ms + blink_gap();
    }
    if (say_pending_) {  // say() não sabe a hora: fixa o prazo no 1º tick
        say_until_ += now_ms;
        say_pending_ = false;
    }
    if (n_lines_ > 0 && say_until_ != 0 && now_ms >= say_until_) {
        n_lines_ = 0;
        say_until_ = 0;
        draw_balloon();
        Rect r = { 0, BALLOON_Y, CANVAS_W, BALLOON_H };
        mark(r);
    }
    bool toggled = false;
    if (!blink_ && now_ms >= next_blink_) {
        blink_ = true;
        blink_end_ = now_ms + BLINK_MS;
        toggled = true;
    } else if (blink_ && now_ms >= blink_end_) {
        blink_ = false;
        next_blink_ = now_ms + blink_gap();
        toggled = true;
    }
    if (toggled) {
        draw_face();
        // dirty = união das caixas dos olhos nos dois quadros (a escala por
        // intensidade nunca sai da caixa original)
        for (int f = 0; f < 2; ++f) {
            for (int l = L_EYE_L; l <= L_EYE_R; ++l) {
                const Sprite& s = SPRITES[expr_][f][l];
                if (!s.px) continue;
                Rect r = { s.x, s.y, s.w, s.h };
                mark(r);
            }
        }
    }
}

}  // namespace face
}  // namespace aracno
