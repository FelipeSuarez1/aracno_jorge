#pragma once
#include <stdint.h>

namespace aracno { namespace hal {

// Watchdog de hardware do HPS (DW_apb_wdt em 0xFFD02000): TORR pré-programado via /dev/mem
// (TOP_INIT = TOP) e depois /dev/watchdog do kernel. Motivo em watchdog.cpp. Processo travado
// ou morto sem magic close → SoC reinicia → U-Boot reconfigura a FPGA → PWM zerados → servos
// soltos. Saída normal faz o magic close (o kernel volta a alimentar; sem reboot).
class Watchdog {
public:
    Watchdog();
    ~Watchdog();
    bool open(uint32_t timeout_s);   // arma; mede o clock; escolhe o período ≥ timeout_s
    bool is_open() const { return regs_ != 0; }
    uint32_t timeout_s() const { return timeout_s_; }
    uint32_t clock_hz() const { return clock_hz_; }
    void kick();
    void close_disarm();             // magic close + libera o mapeamento
private:
    int fd_;
    int wd_fd_;
    uint32_t timeout_s_;
    uint32_t clock_hz_;
    volatile uint8_t* regs_;
};

}}  // namespace aracno::hal
