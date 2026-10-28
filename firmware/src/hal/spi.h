#pragma once
#include <stdint.h>
#include <stddef.h>

namespace aracno { namespace hal {

// Interface de escrita SPI (permite um substituto que captura bytes nos testes de host).
class SpiIo {
public:
    virtual ~SpiIo() {}
    // Escreve n bytes. CS pode subir entre fatias internas (ver SpiDev). false em erro.
    virtual bool write(const uint8_t* data, size_t n) = 0;
    // Escreve e lê simultaneamente (loopback / leitura de registrador).
    virtual bool xfer(const uint8_t* tx, uint8_t* rx, size_t n) = 0;
};

// /dev/spidevB.C via ioctl SPI_IOC_MESSAGE. O spidev do kernel 3.13 limita o TOTAL de cada
// mensagem a `bufsiz` (4096 B por padrão): write() fatia em ioctls separados, e o CS sobe
// entre eles (cada ioctl é uma spi_message; cs_change não se aplica entre ioctls). Só use com
// dispositivos que toleram pausa de CS entre bytes — o ILI9341 tolera (RAMWR continua).
class SpiDev : public SpiIo {
public:
    SpiDev();
    ~SpiDev();
    bool open(const char* dev, uint32_t hz, uint8_t mode = 0, uint8_t bits = 8);
    bool is_open() const { return fd_ >= 0; }
    void close();
    uint32_t hz() const { return hz_; }   // velocidade ACEITA pelo driver (o divisor real pode diferir)
    virtual bool write(const uint8_t* data, size_t n);
    virtual bool xfer(const uint8_t* tx, uint8_t* rx, size_t n);
    static const size_t MAX_CHUNK = 4096;
private:
    int fd_;
    uint32_t hz_;
    SpiDev(const SpiDev&);
    SpiDev& operator=(const SpiDev&);
};

}}  // namespace aracno::hal
