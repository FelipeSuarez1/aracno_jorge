#include "safety/safety.h"

namespace aracno { namespace safety {

const char* reason_name(uint8_t r) {
    static const char* names[N_REASONS] = {"ok", "estop", "travado", "obsoleto", "tombo", "borda", "obstaculo"};
    return r < N_REASONS ? names[r] : "?";
}

static inline bool installed(const Obs& o, int s) { return (o.installed & (1u << s)) != 0; }

static inline bool moves_forward(uint8_t a) { return a == FORWARD || a == LUNGE; }
static inline bool moves_body(uint8_t a) { return a == FORWARD || a == LUNGE || a == TURN_LEFT || a == TURN_RIGHT; }

Verdict Safety::filter(const Obs& o, const Cmd& desired) {
    Verdict v;
    v.cmd = desired;
    v.reason = RS_OK;
    v.abort = false;

    // 1. Botão de parada (NF; 1 = acionado). Trava.
    if (installed(o, S_ESTOP) && o.estop) {
        latched_ = true; latch_reason_ = RS_ESTOP;
        v.cmd.action = STOP; v.reason = RS_ESTOP; v.abort = true;
        return v;
    }
    // 2. Sensor instalado sem leitura recente: não sabemos o que há em volta → solta tudo.
    //    Não trava: quando o sensor voltar, a aranha pode seguir.
    for (int s = 0; s < N_SENSORS; ++s) {
        if (!installed(o, s)) continue;
        if (o.age_ms[s] == AGE_NEVER || o.age_ms[s] > lim_.stale_ms) {
            v.cmd.action = STOP; v.reason = RS_STALE; v.abort = true;
            return v;
        }
    }
    // 3. Tombo: inclinação sustentada por fall_persist ciclos (acelerômetro já filtrado em
    //    Sensors; um tranco de manuseio não trava). Solta os servos e trava.
    if (installed(o, S_ACCEL)) {
        int32_t ax = o.acc_x_mg < 0 ? -static_cast<int32_t>(o.acc_x_mg) : o.acc_x_mg;
        int32_t ay = o.acc_y_mg < 0 ? -static_cast<int32_t>(o.acc_y_mg) : o.acc_y_mg;
        if (ax > lim_.tilt_fall_mg || ay > lim_.tilt_fall_mg) {
            if (fall_ticks_ < 255) ++fall_ticks_;
        } else {
            fall_ticks_ = 0;
        }
        if (fall_ticks_ >= lim_.fall_persist) {
            latched_ = true; latch_reason_ = RS_FALL;
            v.cmd.action = STOP; v.reason = RS_FALL; v.abort = true;
            return v;
        }
    }
    // 4. Trava de evento anterior.
    if (latched_) {
        v.cmd.action = STOP; v.reason = RS_LATCHED; v.abort = true;
        return v;
    }
    // 5. Borda: chão longe demais ou ToF de chão sem leitura válida. Só recuar/parar/cabeça/agachar.
    if (installed(o, S_DIST_FLOOR) && (o.dist_floor_mm == 0 || o.dist_floor_mm > lim_.f_edge_mm)) {
        if (moves_body(desired.action)) { v.cmd.action = STOP; v.reason = RS_EDGE; }
        return v;
    }
    // 6. Obstáculo à frente: nada de avançar/bote; virar e recuar liberados.
    if (installed(o, S_DIST_FRONT) && o.dist_front_mm != 0 && o.dist_front_mm < lim_.d_stop_mm) {
        if (moves_forward(desired.action)) { v.cmd.action = STOP; v.reason = RS_OBSTACLE; }
        return v;
    }
    return v;
}

}}  // namespace aracno::safety
