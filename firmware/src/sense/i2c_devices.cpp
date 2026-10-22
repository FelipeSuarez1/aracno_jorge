#include "sense/i2c_devices.h"

namespace aracno { namespace sense {

// ---- BH1750 ---------------------------------------------------------------
bool Bh1750::init(hal::I2cBus* bus, uint8_t addr) {
    bus_ = bus; addr_ = addr; ok_ = false;
    const uint8_t power_on = 0x01, cont_hres = 0x10;   // datasheet ROHM BH1750 §Instruction Set
    if (!bus_->write(addr_, &power_on, 1)) return false;
    if (!bus_->write(addr_, &cont_hres, 1)) return false;
    ok_ = true;
    return true;
}

bool Bh1750::read(uint16_t* lux) {
    if (!ok_) return false;
    uint8_t d[2];
    if (!bus_->read(addr_, d, 2)) return false;
    uint32_t raw = (static_cast<uint32_t>(d[0]) << 8) | d[1];
    uint32_t l = raw * 10u / 12u;                         // lx = raw / 1,2 (H-res, MTreg padrão)
    *lux = l > 65535u ? 65535u : static_cast<uint16_t>(l);
    return true;
}

// ---- ADXL345 --------------------------------------------------------------
bool Adxl345::init(hal::I2cBus* bus) {
    bus_ = bus; ok_ = false;
    uint8_t id = 0;
    if (!bus_->read_reg8(ADDR, 0x00, &id) || id != 0xE5) return false;   // DEVID
    if (!bus_->write_reg8(ADDR, 0x31, 0x08)) return false;               // DATA_FORMAT: FULL_RES, ±2 g
    if (!bus_->write_reg8(ADDR, 0x2C, 0x0A)) return false;               // BW_RATE: 100 Hz
    if (!bus_->write_reg8(ADDR, 0x2D, 0x08)) return false;               // POWER_CTL: Measure
    ok_ = true;
    return true;
}

bool Adxl345::read(int16_t* x, int16_t* y, int16_t* z) {
    if (!ok_) return false;
    uint8_t reg = 0x32, d[6];                                            // DATAX0..DATAZ1
    if (!bus_->xfer(ADDR, &reg, 1, d, 6)) return false;
    int16_t rx = static_cast<int16_t>(d[0] | (d[1] << 8));
    int16_t ry = static_cast<int16_t>(d[2] | (d[3] << 8));
    int16_t rz = static_cast<int16_t>(d[4] | (d[5] << 8));
    // full-res: 3,9 mg/LSB → mg = raw * 39 / 10
    *x = static_cast<int16_t>(rx * 39 / 10);
    *y = static_cast<int16_t>(ry * 39 / 10);
    *z = static_cast<int16_t>(rz * 39 / 10);
    return true;
}

// ---- PCF8574 --------------------------------------------------------------
bool Pcf8574::init(hal::I2cBus* bus, uint8_t addr) {
    bus_ = bus; addr_ = addr; ok_ = false; shadow_ = 0xFF;
    if (!bus_->write(addr_, &shadow_, 1)) return false;
    uint8_t v;
    if (!bus_->read(addr_, &v, 1)) return false;
    ok_ = true;
    return true;
}

bool Pcf8574::write_bit(int bit, bool v) {
    if (!ok_) return false;
    uint8_t s = shadow_;
    if (v) s |= static_cast<uint8_t>(1u << bit); else s &= static_cast<uint8_t>(~(1u << bit));
    // Entradas (PIR, ESTOP) sempre em 1 para não forçar o pino.
    s |= static_cast<uint8_t>((1u << PIR) | (1u << ESTOP));
    if (!bus_->write(addr_, &s, 1)) return false;
    shadow_ = s;
    return true;
}

bool Pcf8574::read_all(uint8_t* v) {
    if (!ok_) return false;
    return bus_->read(addr_, v, 1);
}

}}  // namespace aracno::sense
