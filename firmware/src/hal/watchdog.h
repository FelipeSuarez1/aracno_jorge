#pragma once
#include <stdint.h>

namespace aracno { namespace hal {

// /dev/watchdog (dw_wdt, CONFIG_DW_WATCHDOG=y no kernel 3.13 da imagem — dia 1).
// Abrir arma o cão; fechar com 'V' ("magic close") desarma. Se o processo morrer
// sem o magic close, o HPS reinicia e o U-Boot reconfigura a FPGA → PWM zerados.
class Watchdog {
public:
    Watchdog();
    ~Watchdog();
    bool open(uint32_t timeout_s);   // tenta WDIOC_SETTIMEOUT; lê o timeout efetivo
    bool is_open() const { return fd_ >= 0; }
    uint32_t timeout_s() const { return timeout_s_; }
    void kick();
    void close_disarm();             // magic close
private:
    int fd_;
    uint32_t timeout_s_;
};

}}  // namespace aracno::hal
