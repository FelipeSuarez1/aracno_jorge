#pragma once
#include <stdint.h>

namespace aracno { namespace medula {

// Modelo de referência bit a bit do bloco `medula` da FPGA (S11, seção "Contrato": algoritmo por quadro).
// Também é o "caminho por software" do reflexo, para a comparação de latência (--medula-test).
// Entrada: profundidade 160×120 em 8 bits (16 mm por unidade, 0 = sem medida), linha a linha.

static const int W = 160, H = 120, N = W * H;

struct Params {
    uint8_t  fg_delta;      // THRESH[7:0]
    uint16_t min_count;     // THRESH[23:8]
    uint8_t  reflex_near;   // REFLEX_CFG[7:0]
    uint8_t  reflex_drop;   // REFLEX_CFG[15:8]
    uint16_t hold_ms;       // REFLEX_CFG[31:16]
    int16_t  pan_gain;      // µs/px × 16, com sinal
    int16_t  tilt_gain;
    uint16_t slew_us;
    bool     reflex_en;     // CTRL bit3: só do modelo (o driver o ignora em configure())
};

// Valores de reset do contrato (8, 40, 38, 4, 1500, 66, 68, 26) com reflex_en = true.
Params default_params();

struct Result {
    bool     target;
    uint16_t count;                       // sempre informado, mesmo sem alvo
    uint8_t  cx, cy, min_depth, mean_depth;
    uint8_t  x0, y0, x1, y1;              // bbox
    bool     reflex;                      // reflexo disparado neste quadro
    uint16_t pan_target_us;               // HEAD_TARGET[15:0]
    uint16_t tilt_target_us;              // HEAD_TARGET[31:16]: efetivo (TILT_MAX_US se houve reflexo neste quadro)
    uint32_t latency_cycles;              // 0 no modelo
};

// Limites fixos do contrato (11,11 µs/°; pan ±60°, tilt ±25° da S06).
static const int PAN_MIN_US = 833, PAN_MAX_US = 2167, TILT_MIN_US = 1222, TILT_MAX_US = 1778, CENTER_US = 1500;

class Model {
public:
    Model();
    void set_frames(uint32_t fi);   // valor de FRAMES no próximo GO (o hardware não zera entre execuções)
    void process(const uint8_t* depth8 /*N*/, bool bg_reset, const Params& p, Result* out);
    uint32_t frames() const { return frames_; }
    uint8_t background(int p) const { return bg_[p]; }
private:
    uint8_t  bg_[N];
    uint32_t frames_;
    bool     prev_target_;
    uint8_t  prev_min_;
    uint16_t pan_t_, tilt_t_;
};

}}  // namespace aracno::medula
