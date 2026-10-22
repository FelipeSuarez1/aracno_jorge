#pragma once
#include <stdint.h>
#include <stddef.h>

namespace aracno { namespace hal {

// Barramento I2C via /dev/i2c-N (ioctl I2C_RDWR). Todas as chamadas são
// bloqueantes no driver DesignWare; o timeout do driver (~1 s em 3.13) é a única
// proteção — por isso o laço marca o sensor como obsoleto por idade, não por erro.
class I2cBus {
public:
    I2cBus();
    ~I2cBus();
    bool open(const char* dev);         // "/dev/i2c-1"
    bool is_open() const { return fd_ >= 0; }
    void close();
    // Escreve `wlen` bytes e, se rlen > 0, lê `rlen` com repeated start. false em erro.
    bool xfer(uint8_t addr, const uint8_t* w, size_t wlen, uint8_t* r, size_t rlen);
    bool write(uint8_t addr, const uint8_t* w, size_t wlen) { return xfer(addr, w, wlen, 0, 0); }
    bool read(uint8_t addr, uint8_t* r, size_t rlen) { return xfer(addr, 0, 0, r, rlen); }
    bool write_reg8(uint8_t addr, uint8_t reg, uint8_t val);
    bool read_reg8(uint8_t addr, uint8_t reg, uint8_t* val);
    // Registrador de 16 bits de endereço (VL53L1X).
    bool write_reg16(uint8_t addr, uint16_t reg, const uint8_t* v, size_t n);
    bool read_reg16(uint8_t addr, uint16_t reg, uint8_t* v, size_t n);
    uint32_t errors() const { return errors_; }

private:
    int fd_;
    uint32_t errors_;
    I2cBus(const I2cBus&);
    I2cBus& operator=(const I2cBus&);
};

}}  // namespace aracno::hal
