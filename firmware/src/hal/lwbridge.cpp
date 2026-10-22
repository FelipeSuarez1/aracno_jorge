#include "hal/lwbridge.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace aracno { namespace hal {

LwBridge::LwBridge() : fd_(-1), base_(0) {}
LwBridge::~LwBridge() { close(); }

bool LwBridge::open() {
    if (base_) return true;
    int fd = ::open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) return false;
    void* p = mmap(0, SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, LWFPGASLVS_OFST);
    if (p == MAP_FAILED) {
        ::close(fd);
        return false;
    }
    fd_ = fd;
    base_ = static_cast<volatile uint8_t*>(p);
    return true;
}

void LwBridge::close() {
    if (base_) {
        munmap(const_cast<uint8_t*>(base_), SPAN);
        base_ = 0;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

static inline uint32_t reg_off(uint32_t comp_base, uint32_t index) {
    return (comp_base + index * 4u) & (LwBridge::SPAN - 1u);
}

void LwBridge::write32(uint32_t comp_base, uint32_t index, uint32_t value) {
    if (!base_) return;
    *reinterpret_cast<volatile uint32_t*>(base_ + reg_off(comp_base, index)) = value;
}

uint32_t LwBridge::read32(uint32_t comp_base, uint32_t index) const {
    if (!base_) return 0;
    return *reinterpret_cast<volatile uint32_t*>(base_ + reg_off(comp_base, index));
}

}}  // namespace aracno::hal
