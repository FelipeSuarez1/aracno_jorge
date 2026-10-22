#include "hal/watchdog.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/watchdog.h>

namespace aracno { namespace hal {

Watchdog::Watchdog() : fd_(-1), timeout_s_(0) {}
Watchdog::~Watchdog() { close_disarm(); }

bool Watchdog::open(uint32_t timeout_s) {
    fd_ = ::open("/dev/watchdog", O_WRONLY);
    if (fd_ < 0) return false;
    int t = static_cast<int>(timeout_s);
    ioctl(fd_, WDIOC_SETTIMEOUT, &t);   // dw_wdt arredonda para a potência de 2 disponível
    int eff = 0;
    if (ioctl(fd_, WDIOC_GETTIMEOUT, &eff) == 0 && eff > 0) timeout_s_ = static_cast<uint32_t>(eff);
    else timeout_s_ = timeout_s;
    kick();
    return true;
}

void Watchdog::kick() {
    if (fd_ >= 0) ioctl(fd_, WDIOC_KEEPALIVE, 0);
}

void Watchdog::close_disarm() {
    if (fd_ >= 0) {
        const char v = 'V';
        ssize_t r = ::write(fd_, &v, 1);
        (void)r;
        ::close(fd_);
        fd_ = -1;
    }
}

}}  // namespace aracno::hal
