#pragma once
#include <stdint.h>
#include <stddef.h>

namespace aracno { namespace hal {

// Interface de escrita SPI (permite um substituto que captura bytes nos testes de host).
// Implementação na placa: hal::AvalonSpi (altera_avalon_spi na FPGA, S08).
class SpiIo {
public:
    virtual ~SpiIo() {}
    virtual bool write(const uint8_t* data, size_t n) = 0;                 // false em erro
    virtual bool xfer(const uint8_t* tx, uint8_t* rx, size_t n) = 0;       // escreve e lê
};

}}  // namespace aracno::hal
