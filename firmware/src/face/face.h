// Face: rosto com humor (7 expressões × 2 quadros, sprites.h) + balão de
// texto (font16.h) desenhados num Canvas 320×240. S04. Sem alocação após a
// construção; sem float. tools/face/render.py --compose replica a composição
// (mesmas constantes abaixo) para gerar as referências de tests/ref/.
#pragma once
#include <stdint.h>
#include "face/canvas.h"

namespace aracno {
namespace face {

enum Expr {
    NEUTRO = 0, ALEGRIA, MEDO, RAIVA, SURPRESA, TRISTEZA, CURIOSIDADE, N_EXPR
};

enum Layer { L_EYE_L = 0, L_EYE_R, L_BROW_L, L_BROW_R, L_MOUTH, N_LAYERS };

// Balão: 320×80 em y=160, contorno 1 px, texto 8×16 em até 3 linhas de 38
// colunas (38×8 = 304 px + margens de 8 px).
static const int BALLOON_Y       = FACE_H;
static const int BALLOON_H       = CANVAS_H - FACE_H;
static const int BALLOON_COLS    = 38;
static const int BALLOON_LINES   = 3;
static const int BALLOON_TEXT_X  = 8;
static const int BALLOON_TEXT_Y  = BALLOON_Y + 8;
static const int BALLOON_LINE_PX = 20;
static const int BALLOON_LINE_BYTES = BALLOON_COLS * 4 + 1;  // UTF-8 pior caso

// Piscar: intervalo pseudo-aleatório 3–5 s (xorshift32, semente fixa),
// olhos fechados por BLINK_MS. [INFERÊNCIA] valores humanos típicos.
static const uint32_t BLINK_MS         = 150;
static const uint32_t BLINK_MIN_GAP_MS = 3000;
static const uint32_t BLINK_MAX_GAP_MS = 5000;
static const uint32_t BLINK_SEED       = 0x2545F491u;

// Quebra de linha: palavras separadas por ' '; palavra > BALLOON_COLS é
// cortada em pedaços de BALLOON_COLS; o que passar de BALLOON_LINES é
// descartado. Colunas contam code points. Devolve o número de linhas.
int wrap_lines(const char* utf8, char out[BALLOON_LINES][BALLOON_LINE_BYTES]);

// Escala (%) aplicada a olhos e boca para a intensidade 0..100: 50..100.
inline int intensity_scale(uint8_t intensity) {
    if (intensity > 100) intensity = 100;
    return 50 + intensity / 2;
}

class Face {
public:
    Face();
    void set(Expr e, uint8_t intensity);          // redesenha tudo
    void say(const char* utf8, uint32_t ms);      // ms == 0: sem timeout
    void tick(uint32_t now_ms);                   // piscar, timeout do balão
    const Canvas& canvas() const { return canvas_; }
    // Região alterada desde o último flushed(); false se nada mudou.
    bool dirty(Rect* out) const;
    void flushed();

    Expr    expr() const { return expr_; }
    uint8_t intensity() const { return intensity_; }
    bool    blinking() const { return blink_; }
    int     lines() const { return n_lines_; }
    const char* line(int i) const { return lines_[i]; }

private:
    void draw_face();
    void draw_balloon();
    void mark(const Rect& r);
    uint32_t rand32();
    uint32_t blink_gap();

    Canvas   canvas_;
    Expr     expr_;
    uint8_t  intensity_;
    bool     blink_;
    bool     started_;
    uint32_t next_blink_;
    uint32_t blink_end_;
    uint32_t say_until_;
    bool     say_pending_;
    uint32_t rng_;
    char     lines_[BALLOON_LINES][BALLOON_LINE_BYTES];
    int      n_lines_;
    Rect     dirty_;
    bool     has_dirty_;
};

}  // namespace face
}  // namespace aracno
