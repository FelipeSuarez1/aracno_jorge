#pragma once
#include <stdint.h>
#include "aracno/types.h"
#include "aracno/discretize.h"

namespace aracno { namespace safety {

// Limites fixos da camada de segurança. Valores iniciais [A MEDIR] (S03: frenagem,
// caracterização dos sensores). Nunca são alterados pela política em tempo de execução.
struct Limits {
    uint16_t d_stop_mm;      // distância frontal mínima para avançar/bote
    uint16_t f_edge_mm;      // leitura do ToF de chão acima disto = borda
    int16_t  tilt_fall_mg;   // |acc_x| ou |acc_y| acima disto = tombo (~600 mg ≈ 37°)
    uint16_t stale_ms;       // idade máxima de uma leitura válida
    Limits() : d_stop_mm(300), f_edge_mm(F_EDGE_MM), tilt_fall_mg(600), stale_ms(200) {}
};

enum Reason {
    RS_OK = 0,
    RS_ESTOP,        // botão de parada
    RS_LATCHED,      // travado desde um evento anterior; exige reset explícito
    RS_STALE,        // sensor instalado sem leitura recente
    RS_FALL,         // inclinação acima do limite
    RS_EDGE,         // borda detectada (ou ToF de chão inválido)
    RS_OBSTACLE,     // objeto à frente abaixo de d_stop
    N_REASONS
};

struct Verdict {
    Cmd     cmd;        // comando permitido
    uint8_t reason;     // Reason da restrição (RS_OK se igual ao desejado)
    bool    abort;      // true = soltar servos (ABORT) agora, não só parar
};

const char* reason_name(uint8_t r);

class Safety {
public:
    explicit Safety(const Limits& l = Limits()) : lim_(l), latched_(false), latch_reason_(RS_OK) {}
    // Avalia o comando desejado contra a observação. Puro: sem I/O.
    Verdict filter(const Obs& o, const Cmd& desired);
    bool latched() const { return latched_; }
    uint8_t latch_reason() const { return latch_reason_; }
    // Só o operador (botão/UART) destrava; a política não tem acesso.
    void reset_latch() { latched_ = false; latch_reason_ = RS_OK; }
    const Limits& limits() const { return lim_; }

private:
    Limits lim_;
    bool latched_;
    uint8_t latch_reason_;
};

}}  // namespace aracno::safety
