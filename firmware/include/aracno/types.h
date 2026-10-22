// Contrato compartilhado entre firmware (C++), simulador (sim/aracno_sim/types.py)
// e testes. Qualquer mudança aqui exige mudança espelhada no Python e nos
// vetores de teste (tests/policy_vectors.h). Ver docs/20-Specs/contrato-obs-cmd.md.
#pragma once
#include <stdint.h>

namespace aracno {

// Ações da política. Mapeiam 1:1 para primitivas de motor/spider.h e head/head.h.
enum Action {
    STOP = 0,
    FORWARD,      // marcha tripé para frente (CSpider::MoveForward)
    BACKWARD,     // (CSpider::MoveBackward)
    TURN_LEFT,    // (CSpider::RotatelLeft)
    TURN_RIGHT,   // (CSpider::RotatelRight)
    CROUCH,       // corpo baixo: TiltNone com joelhos altos (esconder)
    LUNGE,        // bote: velocidade máx + TiltForward + FORWARD curto
    HEAD_LEFT,    // pan da cabeça −20° (S06)
    HEAD_RIGHT,   // pan da cabeça +20° (S06)
    N_ACTIONS
};

struct Cmd {
    uint8_t action;   // Action
    uint8_t speed;    // 10..100 (CSpider::SetSpeed satura nessa faixa)
};

// Índices de sensores para age_ms[] e installed.
enum Sensor {
    S_LUX_L = 0,   // BH1750 esquerda
    S_LUX_R,       // BH1750 direita
    S_DIST_FRONT,  // VL53L1X frontal (na cabeça)
    S_DIST_FLOOR,  // VL53L1X para o chão (borda)
    S_PIR,         // HC-SR501 via PCF8574
    S_ACCEL,       // ADXL345 em /dev/i2c-0 (0x53)
    S_ESTOP,       // botão NF via PCF8574
    N_SENSORS
};

static const uint16_t AGE_NEVER = 0xFFFF;  // nunca lido / não instalado

struct Obs {
    uint32_t t_ms;                  // CLOCK_MONOTONIC em ms no instante da amostra
    uint16_t lux_l, lux_r;          // lux (BH1750 H-res, 1 lx/LSB), saturado em 65535
    uint16_t dist_front_mm;         // 0 = sem leitura válida
    uint16_t dist_floor_mm;         // 0 = sem leitura válida
    uint8_t  pir;                   // 0/1
    uint8_t  estop;                 // 1 = botão de parada acionado (NF aberto)
    int16_t  acc_x_mg, acc_y_mg, acc_z_mg;  // ADXL345, 1 LSB ≈ 3,9 mg em ±2 g
    uint32_t t_since_stimulus_ms;   // desde o último PIR=1 ou objeto < 1 m à frente
    uint16_t age_ms[N_SENSORS];     // idade da última leitura válida; AGE_NEVER se nunca
    uint8_t  installed;             // bitmask (1 << Sensor): sensores presentes nesta montagem
};

}  // namespace aracno
