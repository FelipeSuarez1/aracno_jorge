#include "sense/sensors.h"
#include "hal/clock.h"

namespace aracno { namespace sense {

static inline bool has(uint8_t mask, int s) { return (mask & (1u << s)) != 0; }

Sensors::Sensors() : alive_(0), last_lux_ms_(0), last_stimulus_ms_(0), had_stimulus_(false), last_sample_ms_(0) {
    for (int i = 0; i < N_SENSORS; ++i) { last_ok_[i] = 0; ever_[i] = false; }
}

uint8_t Sensors::init(const SensorConfig& cfg) {
    cfg_ = cfg;
    alive_ = 0;
    const uint8_t want = cfg.installed;
    if (has(want, S_ACCEL) && bus0_.open("/dev/i2c-0") && acc_.init(&bus0_)) alive_ |= 1u << S_ACCEL;

    bool b1 = bus1_.open("/dev/i2c-1");
    if (b1 && (has(want, S_PIR) || has(want, S_ESTOP) || has(want, S_DIST_FRONT) || has(want, S_DIST_FLOOR))) {
        if (pcf_.init(&bus1_, cfg.pcf_addr)) {
            if (has(want, S_PIR)) alive_ |= 1u << S_PIR;
            if (has(want, S_ESTOP)) alive_ |= 1u << S_ESTOP;
        }
    }
    if (b1 && has(want, S_LUX_L) && lux_l_.init(&bus1_, Bh1750::ADDR_L)) alive_ |= 1u << S_LUX_L;
    if (b1 && has(want, S_LUX_R) && lux_r_.init(&bus1_, Bh1750::ADDR_H)) alive_ |= 1u << S_LUX_R;

    // Dois VL53L1X no mesmo barramento: ambos acordam em 0x29. Sequência (ST AN4846):
    // frontal em XSHUT=0, chão em 0x29 → realoca para tof_floor_addr; depois liberta o frontal.
    if (b1 && pcf_.ok() && (has(want, S_DIST_FRONT) || has(want, S_DIST_FLOOR))) {
        pcf_.write_bit(Pcf8574::XSHUT_FRONT, false);
        pcf_.write_bit(Pcf8574::XSHUT_FLOOR, false);
        hal::sleep_ms(10);
        if (has(want, S_DIST_FLOOR)) {
            pcf_.write_bit(Pcf8574::XSHUT_FLOOR, true);
            hal::sleep_ms(10);
            Vl53l1x tmp;
            if (tmp.init(&bus1_, Vl53l1x::DEFAULT_ADDR) && tmp.set_address(cfg.tof_floor_addr) &&
                tof_floor_.init(&bus1_, cfg.tof_floor_addr))
                alive_ |= 1u << S_DIST_FLOOR;
        }
        if (has(want, S_DIST_FRONT)) {
            pcf_.write_bit(Pcf8574::XSHUT_FRONT, true);
            hal::sleep_ms(10);
            if (tof_front_.init(&bus1_, cfg.tof_front_addr)) alive_ |= 1u << S_DIST_FRONT;
        }
    } else if (b1 && !pcf_.ok() && has(want, S_DIST_FRONT) && !has(want, S_DIST_FLOOR)) {
        // Sem PCF8574 só cabe um ToF, no endereço padrão.
        if (tof_front_.init(&bus1_, cfg.tof_front_addr)) alive_ |= 1u << S_DIST_FRONT;
    }
    return alive_;
}

void Sensors::ages(Obs* o, uint32_t now_ms) {
    for (int s = 0; s < N_SENSORS; ++s) {
        if (!has(cfg_.installed, s) || !ever_[s]) { o->age_ms[s] = AGE_NEVER; continue; }
        uint32_t a = now_ms - last_ok_[s];
        o->age_ms[s] = a > 0xFFFEu ? 0xFFFEu : static_cast<uint16_t>(a);
    }
}

void Sensors::sample(Obs* o, uint32_t now_ms) {
    o->t_ms = now_ms;
    o->installed = cfg_.installed;

    if (has(alive_, S_ACCEL)) {
        int16_t x, y, z;
        if (acc_.read(&x, &y, &z)) { o->acc_x_mg = x; o->acc_y_mg = y; o->acc_z_mg = z; stamp(o, S_ACCEL, now_ms); }
    }
    if ((has(alive_, S_PIR) || has(alive_, S_ESTOP))) {
        uint8_t v;
        if (pcf_.read_all(&v)) {
            if (has(alive_, S_PIR))   { o->pir = (v >> Pcf8574::PIR) & 1; stamp(o, S_PIR, now_ms); }
            if (has(alive_, S_ESTOP)) { o->estop = (v >> Pcf8574::ESTOP) & 1; stamp(o, S_ESTOP, now_ms); }
        }
    }
    // BH1750 H-res converte a cada ~120 ms; ler mais rápido devolve o mesmo valor.
    if (now_ms - last_lux_ms_ >= 120u) {
        last_lux_ms_ = now_ms;
        uint16_t l;
        if (has(alive_, S_LUX_L) && lux_l_.read(&l)) { o->lux_l = l; stamp(o, S_LUX_L, now_ms); }
        if (has(alive_, S_LUX_R) && lux_r_.read(&l)) { o->lux_r = l; stamp(o, S_LUX_R, now_ms); }
    }
    uint16_t mm;
    if (has(alive_, S_DIST_FRONT) && tof_front_.poll(&mm)) { o->dist_front_mm = mm; stamp(o, S_DIST_FRONT, now_ms); }
    if (has(alive_, S_DIST_FLOOR) && tof_floor_.poll(&mm)) { o->dist_floor_mm = mm; stamp(o, S_DIST_FLOOR, now_ms); }

    // Estímulo: PIR ativo ou objeto a menos de 1 m à frente.
    if (o->pir || (o->dist_front_mm != 0 && o->dist_front_mm < 1000u)) { last_stimulus_ms_ = now_ms; had_stimulus_ = true; }
    o->t_since_stimulus_ms = had_stimulus_ ? now_ms - last_stimulus_ms_ : 0xFFFFFFFFu;

    // Idades calculadas DEPOIS das transações: se o I2C bloqueou (timeout do DesignWare ≈ 1 s),
    // a leitura "fresca" já é velha e a segurança precisa ver isso.
    uint32_t t1 = hal::now_ms();
    ages(o, t1);
    last_sample_ms_ = t1 - now_ms;
}

}}  // namespace aracno::sense
