// Discretização de Obs para a política tabular. Espelhada em sim/aracno_sim/discretize.py.
// Os limites são inclusivos à direita no nível inferior: valor <= limite[i] → nível i.
#pragma once
#include <stdint.h>
#include "aracno/types.h"

namespace aracno {

// Lux: 4 níveis — escuro / penumbra / claro / muito claro.
static const uint16_t LUX_BINS[3] = {10, 50, 200};
static const uint8_t  LUX_LEVELS = 4;

// Distância frontal: 5 níveis; 0 (inválido) conta como "longe" (nível 4).
static const uint16_t DIST_FRONT_BINS[4] = {150, 300, 600, 1200};
static const uint8_t  DIST_FRONT_LEVELS = 5;

// Chão: 2 níveis — chão presente / borda (ou inválido). F_EDGE_MM é [A MEDIR] (S03/S05).
static const uint16_t F_EDGE_MM = 120;
static const uint8_t  FLOOR_LEVELS = 2;

static const uint8_t  PIR_LEVELS = 2;

// Tempo desde o último estímulo: 3 níveis.
static const uint32_t T_SINCE_BINS[2] = {2000, 10000};
static const uint8_t  T_SINCE_LEVELS = 3;

static const uint16_t N_STATES = LUX_LEVELS * LUX_LEVELS * DIST_FRONT_LEVELS * FLOOR_LEVELS * PIR_LEVELS * T_SINCE_LEVELS;  // 960

inline uint8_t lux_level(uint16_t lux) {
    uint8_t l = 0;
    while (l < 3 && lux > LUX_BINS[l]) ++l;
    return l;
}

inline uint8_t dist_front_level(uint16_t mm) {
    if (mm == 0) return DIST_FRONT_LEVELS - 1;
    uint8_t l = 0;
    while (l < 4 && mm > DIST_FRONT_BINS[l]) ++l;
    return l;
}

inline uint8_t floor_level(uint16_t mm) {
    return (mm == 0 || mm > F_EDGE_MM) ? 1 : 0;
}

inline uint8_t t_since_level(uint32_t ms) {
    uint8_t l = 0;
    while (l < 2 && ms > T_SINCE_BINS[l]) ++l;
    return l;
}

// Índice do estado: ordem (lux_l, lux_r, dist_front, floor, pir, t_since), último varia mais rápido.
inline uint16_t state_index(const Obs& o) {
    uint16_t s = lux_level(o.lux_l);
    s = s * LUX_LEVELS + lux_level(o.lux_r);
    s = s * DIST_FRONT_LEVELS + dist_front_level(o.dist_front_mm);
    s = s * FLOOR_LEVELS + floor_level(o.dist_floor_mm);
    s = s * PIR_LEVELS + (o.pir ? 1 : 0);
    s = s * T_SINCE_LEVELS + t_since_level(o.t_since_stimulus_ms);
    return s;
}

}  // namespace aracno
