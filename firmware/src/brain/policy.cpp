#include "brain/policy.h"
#include "aracno/discretize.h"
#include "brain/weights.h"
#include "brain/policy_vectors.h"

namespace aracno {

static_assert(WEIGHTS_N_STATES == N_STATES, "weights.h gerado com N_STATES diferente de discretize.h");
static_assert(WEIGHTS_N_ACTIONS == N_ACTIONS, "weights.h gerado com N_ACTIONS diferente de types.h");
static_assert(sizeof(Q_ARGMAX) / sizeof(Q_ARGMAX[0]) == N_STATES, "Q_ARGMAX com tamanho errado");
static_assert(sizeof(POLICY_VECTORS) / sizeof(POLICY_VECTORS[0]) == N_POLICY_VECTORS, "POLICY_VECTORS com tamanho errado");

static const uint8_t SPEED_NORMAL = 50;
static const uint8_t SPEED_LUNGE = 100;

Cmd Policy::act(const Obs& o) const {
    const uint16_t s = state_index(o);
    Cmd c;
    c.action = Q_ARGMAX[s];
    c.speed = (c.action == LUNGE) ? SPEED_LUNGE : SPEED_NORMAL;
    return c;
}

const char* Policy::weights_sha() const {
    return WEIGHTS_SHA;
}

bool Policy::self_test() const {
    for (uint16_t s = 0; s < N_STATES; ++s) {
        if (Q_ARGMAX[s] >= N_ACTIONS) return false;
    }
    Obs o = Obs();
    for (uint16_t i = 0; i < N_POLICY_VECTORS; ++i) {
        const PolicyVector& v = POLICY_VECTORS[i];
        o.lux_l = v.lux_l;
        o.lux_r = v.lux_r;
        o.dist_front_mm = v.dist_front_mm;
        o.dist_floor_mm = v.dist_floor_mm;
        o.pir = v.pir;
        o.t_since_stimulus_ms = v.t_since_stimulus_ms;
        if (state_index(o) != v.state) return false;
        const Cmd c = act(o);
        if (c.action != v.action) return false;
        if (c.speed != (v.action == LUNGE ? SPEED_LUNGE : SPEED_NORMAL)) return false;
    }
    return true;
}

}  // namespace aracno
