#include "motor/pwm.h"

namespace aracno { namespace motor {

enum { REG_TOTAL_DUR = 0, REG_HIGH_DUR = 1, REG_ADJ_SPEED = 2, REG_STATUS = 2, REG_ABORT = 3 };

// hps_0.h (Spider CD): PWM17_BASE 0x0 … PWM0_BASE 0x110, 16 bytes cada; CMotor.cpp
// indexa szPWM_Base[motor] = 0x110 - 0x10*motor.
uint32_t PwmBank::base_of(int motor) {
    return 0x110u - 0x10u * static_cast<uint32_t>(motor);
}

uint32_t PwmBank::angle_to_high_dur(float angle_deg, bool mirror) {
    // CMotor::Move: PWM = (angle - MIN_ANGLE)/(MAX-MIN) * (MAX_PWM - MIN_PWM) + MIN_PWM,
    // com angle negado para motores < 9. DEGREE_MIN/MAX = ±90.
    if (mirror) angle_deg = -angle_deg;
    if (angle_deg > 90.0f) angle_deg = 90.0f;
    if (angle_deg < -90.0f) angle_deg = -90.0f;
    float span = static_cast<float>(HIGH_MAX - HIGH_MIN);
    return static_cast<uint32_t>((angle_deg + 90.0f) / 180.0f * span) + HIGH_MIN;
}

uint32_t PwmBank::divider_for_speed(int speed) {
    // CMotor::SetSpeed: satura em [0, 99]; m_Speed = ((MIN-MAX)/100)*(100-Speed) + MAX.
    if (speed > 100) speed = 99;
    if (speed < 0) speed = 0;
    return ((DIV_SLOW - DIV_FAST) / 100u) * static_cast<uint32_t>(100 - speed) + DIV_FAST;
}

uint32_t PwmBank::ramp_ms(uint32_t from_high, uint32_t to_high, uint32_t divider) {
    // TERASIC_PWM_EX.v + PWM_Delay.v: high_dur_temp avança 1 a cada (divider+1) ciclos de 50 MHz.
    uint32_t delta = from_high > to_high ? from_high - to_high : to_high - from_high;
    uint64_t clocks = static_cast<uint64_t>(delta) * (divider + 1u);
    return static_cast<uint32_t>(clocks * 1000u / CLOCK_HZ);
}

PwmBank::PwmBank() : b_(0) {
    for (int i = 0; i < N; ++i) {
        target_[i] = 0.0f;
        high_[i] = HIGH_ZERO;
        div_[i] = DIV_DEFAULT;
        eta_[i] = 0;
    }
}

void PwmBank::init(hal::RegIo* b) {
    b_ = b;
    for (int i = 0; i < N; ++i) {
        b_->write32(base_of(i), REG_TOTAL_DUR, TOTAL_DUR);
        b_->write32(base_of(i), REG_ADJ_SPEED, DIV_DEFAULT);
        b_->write32(base_of(i), REG_ABORT, 0);
        div_[i] = DIV_DEFAULT;
        // Leitura de HIGH_DUR devolve high_dur_temp (TERASIC_PWM_EX.v): posição real atual,
        // mesmo se outro processo (spider) deixou os servos num ângulo qualquer.
        uint32_t cur = b_->read32(base_of(i), REG_HIGH_DUR);
        high_[i] = (cur >= HIGH_MIN && cur <= HIGH_MAX) ? cur : HIGH_ZERO;
        target_[i] = (static_cast<float>(high_[i] - HIGH_MIN) / static_cast<float>(HIGH_MAX - HIGH_MIN)) * 180.0f - 90.0f;
        if (i < 9) target_[i] = -target_[i];
        eta_[i] = 0;
    }
}

void PwmBank::move(int motor, float angle_deg, uint32_t now_ms) {
    if (motor < 0 || motor >= N || !b_) return;
    uint32_t h = angle_to_high_dur(angle_deg, motor < 9);
    eta_[motor] = now_ms + ramp_ms(high_[motor], h, div_[motor]) + 40u;  // +2 períodos de 20 ms
    high_[motor] = h;
    target_[motor] = angle_deg;
    b_->write32(base_of(motor), REG_HIGH_DUR, h);
}

bool PwmBank::ready(int motor) const {
    if (motor < 0 || motor >= N || !b_) return true;
    return (b_->read32(base_of(motor), REG_STATUS) & 1u) != 0;
}

void PwmBank::set_speed(int motor, int speed) {
    if (motor < 0 || motor >= N || !b_) return;
    div_[motor] = divider_for_speed(speed);
    b_->write32(base_of(motor), REG_ADJ_SPEED, div_[motor]);
}

void PwmBank::set_speed_all(int speed) {
    for (int i = 0; i < N; ++i) set_speed(i, speed);
}

void PwmBank::abort(int motor, bool on) {
    if (motor < 0 || motor >= N || !b_) return;
    b_->write32(base_of(motor), REG_ABORT, on ? 1u : 0u);
}

void PwmBank::abort_all(bool on) {
    for (int i = 0; i < N; ++i) abort(i, on);
}

}}  // namespace aracno::motor
