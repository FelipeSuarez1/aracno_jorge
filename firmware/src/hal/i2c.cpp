#include "hal/i2c.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

namespace aracno { namespace hal {

I2cBus::I2cBus() : fd_(-1), errors_(0) {}
I2cBus::~I2cBus() { close(); }

bool I2cBus::open(const char* dev) {
    close();
    fd_ = ::open(dev, O_RDWR);
    return fd_ >= 0;
}

void I2cBus::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool I2cBus::xfer(uint8_t addr, const uint8_t* w, size_t wlen, uint8_t* r, size_t rlen) {
    if (fd_ < 0) return false;
    i2c_msg msgs[2];
    int n = 0;
    if (wlen) {
        msgs[n].addr = addr;
        msgs[n].flags = 0;
        msgs[n].len = static_cast<uint16_t>(wlen);
        msgs[n].buf = const_cast<uint8_t*>(w);
        ++n;
    }
    if (rlen) {
        msgs[n].addr = addr;
        msgs[n].flags = I2C_M_RD;
        msgs[n].len = static_cast<uint16_t>(rlen);
        msgs[n].buf = r;
        ++n;
    }
    if (!n) return true;
    i2c_rdwr_ioctl_data d;
    d.msgs = msgs;
    d.nmsgs = static_cast<uint32_t>(n);
    if (ioctl(fd_, I2C_RDWR, &d) != n) {
        ++errors_;
        return false;
    }
    return true;
}

bool I2cBus::write_reg8(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t b[2] = {reg, val};
    return write(addr, b, 2);
}

bool I2cBus::read_reg8(uint8_t addr, uint8_t reg, uint8_t* val) {
    return xfer(addr, &reg, 1, val, 1);
}

bool I2cBus::write_reg16(uint8_t addr, uint16_t reg, const uint8_t* v, size_t n) {
    uint8_t b[2 + 8];
    if (n > 8) return false;
    b[0] = static_cast<uint8_t>(reg >> 8);
    b[1] = static_cast<uint8_t>(reg & 0xFF);
    for (size_t i = 0; i < n; ++i) b[2 + i] = v[i];
    return write(addr, b, 2 + n);
}

bool I2cBus::read_reg16(uint8_t addr, uint16_t reg, uint8_t* v, size_t n) {
    uint8_t b[2] = {static_cast<uint8_t>(reg >> 8), static_cast<uint8_t>(reg & 0xFF)};
    return xfer(addr, b, 2, v, n);
}

}}  // namespace aracno::hal
