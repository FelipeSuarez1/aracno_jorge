#include "hal/spi.h"
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/spi/spidev.h>

namespace aracno { namespace hal {

SpiDev::SpiDev() : fd_(-1), hz_(0) {}
SpiDev::~SpiDev() { close(); }

bool SpiDev::open(const char* dev, uint32_t hz, uint8_t mode, uint8_t bits) {
    close();
    fd_ = ::open(dev, O_RDWR);
    if (fd_ < 0) return false;
    if (ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0) {
        close();
        return false;
    }
    uint32_t eff = 0;
    hz_ = (ioctl(fd_, SPI_IOC_RD_MAX_SPEED_HZ, &eff) == 0 && eff) ? eff : hz;
    return true;
}

void SpiDev::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool SpiDev::xfer(const uint8_t* tx, uint8_t* rx, size_t n) {
    if (fd_ < 0) return false;
    size_t off = 0;
    while (off < n) {
        size_t len = n - off; if (len > MAX_CHUNK) len = MAX_CHUNK;
        spi_ioc_transfer t;
        std::memset(&t, 0, sizeof t);
        t.tx_buf = reinterpret_cast<unsigned long>(tx + off);
        t.rx_buf = rx ? reinterpret_cast<unsigned long>(rx + off) : 0;
        t.len = static_cast<uint32_t>(len);
        t.speed_hz = hz_;
        t.bits_per_word = 8;
        t.cs_change = 0;   // só afeta transfers dentro da mesma mensagem (aqui há uma)
        if (ioctl(fd_, SPI_IOC_MESSAGE(1), &t) < 0) return false;
        off += len;
    }
    return true;
}

bool SpiDev::write(const uint8_t* data, size_t n) { return xfer(data, 0, n); }

}}  // namespace aracno::hal
