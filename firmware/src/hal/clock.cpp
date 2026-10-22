#include "hal/clock.h"
#include <time.h>

namespace aracno { namespace hal {

uint32_t now_ms() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

uint64_t now_us() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000u + ts.tv_nsec / 1000u;
}

void sleep_until_us(uint64_t t_us) {
    timespec ts;
    ts.tv_sec = static_cast<time_t>(t_us / 1000000u);
    ts.tv_nsec = static_cast<long>((t_us % 1000000u) * 1000u);
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, 0) != 0) {
        // EINTR: repete
    }
}

void sleep_ms(uint32_t ms) {
    sleep_until_us(now_us() + static_cast<uint64_t>(ms) * 1000u);
}

}}  // namespace aracno::hal
