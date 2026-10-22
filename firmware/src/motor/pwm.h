#pragma once
#include <stdint.h>
#include "hal/lwbridge.h"

namespace aracno { namespace motor {

// 18 controladores TERASIC_PWM_EX na ponte lightweight (Spider CD: hps_0.h, CMotor.cpp,
// ip/spider/TERASIC_PWM_EX.v). Registradores por canal (índice ×4 bytes):
//   0 TOTAL_DUR  período em ticks de 50 MHz (1 000 000 = 20 ms)
//   1 HIGH_DUR   largura alvo; o IP rampa high_dur_temp 1 tick a cada (ADJ_SPEED+1) clocks
//   2 ADJ_SPEED  (escrita) divisor da rampa / STATUS (leitura) bit0 = alvo atingido
//   3 ABORT      1 = saída PWM em 0 (servo solto) a partir do fim do período corrente
// Pulso: 25 000 (0,5 ms) ↔ −90°, 75 000 (1,5 ms) ↔ 0°, 125 000 (2,5 ms) ↔ +90°.
// Motores 0–8 (pernas direitas) são espelhados: ângulo → −ângulo (CMotor::Move).
// O IP só começa a rampar depois de receber HIGH_DUR == 75 000 uma vez (sinal `init`
// em TERASIC_PWM_EX.v); por isso init() escreve 0° em todos os canais.
class PwmBank {
public:
    static const int N = 18;
    static const uint32_t CLOCK_HZ    = 50000000u;
    static const uint32_t TOTAL_DUR   = CLOCK_HZ / 50;          // 1 000 000 = 20 ms
    static const uint32_t HIGH_MIN    = TOTAL_DUR * 5 / 200;    // 25 000 = 0,5 ms → −90°
    static const uint32_t HIGH_ZERO   = TOTAL_DUR * 15 / 200;   // 75 000 = 1,5 ms → 0°
    static const uint32_t HIGH_MAX    = TOTAL_DUR * 25 / 200;   // 125 000 = 2,5 ms → +90°
    static const uint32_t DIV_FAST    = 550;    // CMotor PWM_SPEED_MAX (speed 100)
    static const uint32_t DIV_SLOW    = 1150;   // CMotor PWM_SPEED_MIN (speed 0)
    static const uint32_t DIV_DEFAULT = DIV_FAST + DIV_SLOW;  // 1700, escrito pelo construtor de CMotor

    PwmBank();
    // Aplica TOTAL_DUR, ADJ_SPEED=DIV_DEFAULT e ABORT=0 em todos (como CMotor::CMotor) e
    // semeia o alvo conhecido lendo HIGH_DUR (o IP devolve high_dur_temp). NÃO arma a rampa:
    // o armamento (HIGH_DUR=75000, um servo por vez, 100 ms) é feito pelos 18 primeiros passos
    // de M_STAND (CSpider::Init), que passam pela camada de segurança.
    void init(hal::RegIo* b);
    // Ângulo em graus (−90..+90, satura). Espelha motores 0–8. Guarda alvo e instante.
    void move(int motor, float angle_deg, uint32_t now_ms);
    bool ready(int motor) const;                 // STATUS bit0
    void set_speed(int motor, int speed_0_100);  // CMotor::SetSpeed
    void set_speed_all(int speed_0_100);
    void abort(int motor, bool on);              // ABORT=1 → servo solto
    void abort_all(bool on);
    float target_deg(int motor) const { return target_[motor]; }
    // Estimativa de término do último move (ms absolutos): |Δhigh_dur|·(div+1)/50 MHz + margem.
    uint32_t eta_ms(int motor) const { return eta_[motor]; }

    static uint32_t angle_to_high_dur(float angle_deg, bool mirror);
    static uint32_t divider_for_speed(int speed_0_100);
    static uint32_t ramp_ms(uint32_t from_high, uint32_t to_high, uint32_t divider);
    static uint32_t base_of(int motor);          // offset Qsys do canal (hps_0.h)

private:
    hal::RegIo* b_;
    float    target_[N];
    uint32_t high_[N];
    uint32_t div_[N];
    uint32_t eta_[N];
};

}}  // namespace aracno::motor
