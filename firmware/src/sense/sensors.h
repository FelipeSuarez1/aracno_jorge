#pragma once
#include <stdint.h>
#include "aracno/types.h"
#include "hal/i2c.h"
#include "sense/i2c_devices.h"
#include "sense/vl53l1x.h"

namespace aracno { namespace sense {

// Quais sensores existem nesta montagem (bitmask de Sensor). Ausentes não contam
// como obsoletos para a camada de segurança; presentes e sem leitura recente contam.
struct SensorConfig {
    uint8_t installed;      // (1 << S_LUX_L) | ...
    uint8_t pcf_addr;       // 0x20
    uint8_t tof_front_addr; // 0x29 (padrão) — o do chão é realocado para tof_floor_addr via XSHUT
    uint8_t tof_floor_addr; // 0x2A
    SensorConfig() : installed(0), pcf_addr(0x20), tof_front_addr(0x29), tof_floor_addr(0x2A) {}
};

class Sensors {
public:
    Sensors();
    // Abre /dev/i2c-0 (ADXL345) e /dev/i2c-1 (resto) e inicializa os sensores de cfg.installed.
    // Devolve a máscara dos que responderam; os que falharam ficam com age = AGE_NEVER.
    uint8_t init(const SensorConfig& cfg);
    // Lê o que estiver pronto e atualiza `obs` (idades, t_since_stimulus). Chamar a cada ciclo.
    void sample(Obs* obs, uint32_t now_ms);
    uint32_t i2c_errors() const { return bus0_.errors() + bus1_.errors(); }
    // Duração da última sample() em ms (I2C é bloqueante nesta thread; S03 mede e limita).
    uint32_t last_sample_ms() const { return last_sample_ms_; }
    Pcf8574& pcf() { return pcf_; }

private:
    void stamp(Obs* o, int s, uint32_t now_ms) { last_ok_[s] = now_ms; ever_[s] = true; (void)o; }
    void ages(Obs* o, uint32_t now_ms);

    SensorConfig cfg_;
    uint8_t alive_;
    hal::I2cBus bus0_, bus1_;
    Bh1750 lux_l_, lux_r_;
    Vl53l1x tof_front_, tof_floor_;
    Adxl345 acc_;
    Pcf8574 pcf_;
    uint32_t last_ok_[N_SENSORS];
    bool ever_[N_SENSORS];
    uint32_t last_lux_ms_;
    uint32_t last_stimulus_ms_;
    bool had_stimulus_;
    uint32_t last_sample_ms_;
};

}}  // namespace aracno::sense
