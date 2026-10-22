#pragma once
#include <stdint.h>
#include "hal/i2c.h"

namespace aracno { namespace sense {

// BH1750 (ROHM): modo contínuo H-res (1 lx/LSB, ~120 ms por amostra). Endereços 0x23 (ADDR=L) / 0x5C (ADDR=H).
class Bh1750 {
public:
    static const uint8_t ADDR_L = 0x23, ADDR_H = 0x5C;
    Bh1750() : bus_(0), addr_(ADDR_L), ok_(false) {}
    bool init(hal::I2cBus* bus, uint8_t addr);   // power on + CONT_H_RES (0x10)
    bool read(uint16_t* lux);                      // lux = raw / 1.2 (datasheet), saturado
    bool ok() const { return ok_; }
private:
    hal::I2cBus* bus_; uint8_t addr_; bool ok_;
};

// ADXL345 na DE0-Nano-SoC: /dev/i2c-0, 0x53 (dia 1: i2cdetect). Saída ±2 g, 3,9 mg/LSB em full-res.
class Adxl345 {
public:
    static const uint8_t ADDR = 0x53;
    Adxl345() : bus_(0), ok_(false) {}
    bool init(hal::I2cBus* bus);                   // DEVID 0xE5; POWER_CTL measure; DATA_FORMAT full-res ±2 g
    bool read(int16_t* x_mg, int16_t* y_mg, int16_t* z_mg);
    bool ok() const { return ok_; }
private:
    hal::I2cBus* bus_; bool ok_;
};

// PCF8574 (0x20..0x27): 8 I/O quasi-bidirecionais. Bits de entrada ficam em 1 (pull-up interno fraco).
// Mapa do projeto (S03): P0 XSHUT ToF frontal, P1 XSHUT ToF chão, P2 PIR (entrada),
// P3 botão NF de parada (entrada; 1 = aberto = acionado), P4 D/C display, P5 RST display, P6–P7 livres.
class Pcf8574 {
public:
    enum { XSHUT_FRONT = 0, XSHUT_FLOOR = 1, PIR = 2, ESTOP = 3, LCD_DC = 4, LCD_RST = 5 };
    Pcf8574() : bus_(0), addr_(0x20), shadow_(0xFF), ok_(false) {}
    bool init(hal::I2cBus* bus, uint8_t addr);   // escreve 0xFF (tudo entrada/alto) e lê de volta
    bool write_bit(int bit, bool v);               // mantém entradas em 1
    bool read_all(uint8_t* v);
    uint8_t shadow() const { return shadow_; }
    bool ok() const { return ok_; }
private:
    hal::I2cBus* bus_; uint8_t addr_; uint8_t shadow_; bool ok_;
};

}}  // namespace aracno::sense
