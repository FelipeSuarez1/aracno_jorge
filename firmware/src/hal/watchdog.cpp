#include "hal/watchdog.h"
#include "hal/clock.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

namespace aracno { namespace hal {

// DW_apb_wdt (Synopsys) do HPS: wd@ffd02000 (dia 1, DTB). Registradores (Cyclone V HPS TRM, "Watchdog Timer"):
//   0x00 CR   bit0 WDT_EN (uma vez ligado, só reset desliga), bit1 RMOD (0 = reset direto)
//   0x04 TORR bits 3:0 TOP (período = 2^(16+TOP) ciclos), bits 7:4 TOP_INIT (1º período após ligar)
//   0x08 CCVR contador corrente
//   0x0C CRR  escrever 0x76 reinicia o contador
// Dois fatos observados na placa (2026-10-22): (1) o dw_wdt do kernel 3.13 escreve só TOP e deixa
// TOP_INIT = 0 → abrir /dev/watchdog reinicia o SoC em ~2,6 ms, antes do 1º kick; (2) enquanto
// ninguém abre /dev/watchdog, um timer do kernel alimenta o cão sozinho — programar os
// registradores sem abrir o device não dá proteção nenhuma (o kernel continua alimentando).
// Solução: pré-programar TORR com TOP_INIT = TOP pelos registradores e SÓ ENTÃO abrir
// /dev/watchdog (o kernel para de alimentar; nossos kicks passam a ser obrigatórios). O magic
// close na saída devolve o cão ao kernel — sem reboot a cada execução.
static const uint32_t WDT_BASE = 0xFFD02000u;
enum { CR = 0x00, TORR = 0x04, CCVR = 0x08, CRR = 0x0C };
static const uint32_t RESTART_KEY = 0x76;

Watchdog::Watchdog() : fd_(-1), wd_fd_(-1), timeout_s_(0), clock_hz_(0), regs_(0) {}
Watchdog::~Watchdog() { close_disarm(); }

static inline volatile uint32_t* reg(volatile uint8_t* b, uint32_t off) {
    return reinterpret_cast<volatile uint32_t*>(b + off);
}

bool Watchdog::open(uint32_t timeout_s) {
    fd_ = ::open("/dev/mem", O_RDWR | O_SYNC);
    if (fd_ < 0) return false;
    void* p = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, WDT_BASE);
    if (p == MAP_FAILED) { ::close(fd_); fd_ = -1; return false; }
    regs_ = static_cast<volatile uint8_t*>(p);

    // 1. Mede o clock do contador (o kernel já o mantém ligado e alimentado): Δ CCVR em 100 ms.
    uint32_t c0 = *reg(regs_, CCVR);
    sleep_ms(100);
    uint32_t c1 = *reg(regs_, CCVR);
    uint32_t hz = (c0 > c1) ? (c0 - c1) * 10u : 25000000u;   // conta para baixo
    clock_hz_ = hz;

    // 2. Menor TOP cujo período ≥ timeout; escreve TOP_INIT = TOP (o driver só escreve TOP).
    uint32_t top = 0;
    while (top < 15 && (static_cast<uint64_t>(1) << (16 + top)) < static_cast<uint64_t>(hz) * timeout_s) ++top;
    *reg(regs_, TORR) = (top << 4) | top;
    *reg(regs_, CR) = 0x1;                 // WDT_EN, RMOD=0 (reset) — idempotente se já ligado
    *reg(regs_, CRR) = RESTART_KEY;

    // 3. Agora o driver: o kernel para de alimentar; o timeout pedido é reaplicado por ioctl.
    wd_fd_ = ::open("/dev/watchdog", O_WRONLY);
    if (wd_fd_ < 0) { close_disarm(); return false; }
    int t = static_cast<int>(timeout_s);
    ioctl(wd_fd_, WDIOC_SETTIMEOUT, &t);
    *reg(regs_, TORR) = (top << 4) | top;  // o SETTIMEOUT zera TOP_INIT de novo; repõe
    int eff = 0;
    if (ioctl(wd_fd_, WDIOC_GETTIMEOUT, &eff) == 0 && eff > 0) timeout_s_ = static_cast<uint32_t>(eff);
    else timeout_s_ = static_cast<uint32_t>((static_cast<uint64_t>(1) << (16 + top)) / hz);
    kick();
    return true;
}

void Watchdog::kick() {
    if (wd_fd_ >= 0) ioctl(wd_fd_, WDIOC_KEEPALIVE, 0);
    else if (regs_) *reg(regs_, CRR) = RESTART_KEY;
}

void Watchdog::close_disarm() {
    // Magic close: o driver volta a alimentar o cão sozinho (sem reboot ao sair).
    if (wd_fd_ >= 0) {
        const char v = 'V';
        ssize_t r = ::write(wd_fd_, &v, 1); (void)r;
        ::close(wd_fd_); wd_fd_ = -1;
    }
    if (regs_) { munmap(const_cast<uint8_t*>(regs_), 4096); regs_ = 0; }
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

}}  // namespace aracno::hal
