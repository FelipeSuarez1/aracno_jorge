#include "hal/gpio_sysfs.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace aracno { namespace hal {

static bool write_str(const char* path, const char* s) {
    int fd = ::open(path, O_WRONLY);
    if (fd < 0) return false;
    ssize_t n = ::write(fd, s, std::strlen(s));
    ::close(fd);
    return n == static_cast<ssize_t>(std::strlen(s));
}

bool gpio_set_output(int gpio, bool value) {
    char path[64], num[16];
    std::snprintf(num, sizeof num, "%d", gpio);
    std::snprintf(path, sizeof path, "/sys/class/gpio/gpio%d/direction", gpio);
    if (::access(path, F_OK) != 0) {
        if (!write_str("/sys/class/gpio/export", num)) return false;
    }
    // "low"/"high" fixa direção e valor numa escrita só (sem glitch).
    if (!write_str(path, value ? "high" : "low")) return false;
    std::snprintf(path, sizeof path, "/sys/class/gpio/gpio%d/value", gpio);
    return write_str(path, value ? "1" : "0");
}

}}  // namespace aracno::hal
