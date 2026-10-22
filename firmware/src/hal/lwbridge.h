#pragma once
#include <stdint.h>

namespace aracno { namespace hal {

// Acesso a registradores de 32 bits de um componente Qsys. Interface para permitir
// um substituto em memória nos testes de host.
class RegIo {
public:
    virtual ~RegIo() {}
    virtual void     write32(uint32_t comp_base, uint32_t index, uint32_t value) = 0;
    virtual uint32_t read32(uint32_t comp_base, uint32_t index) const = 0;
};

// Ponte lightweight HPS→FPGA via /dev/mem. Porta de CMMap (Spider CD, mmap.cpp):
// janela de 64 MiB a partir de ALT_STM_OFST (0xFC000000); registradores Qsys em
// ALT_LWFPGASLVS_OFST (0xFF200000) + base do componente (hps_0.h) + índice*4.
class LwBridge : public RegIo {
public:
    LwBridge();
    ~LwBridge();
    bool open();                 // false se /dev/mem ou mmap falhar
    bool is_open() const { return base_ != 0; }
    void close();
    virtual void     write32(uint32_t comp_base, uint32_t index, uint32_t value);
    virtual uint32_t read32(uint32_t comp_base, uint32_t index) const;

    static const uint32_t STM_OFST        = 0xFC000000u;  // hwlib socal/hps.h ALT_STM_OFST
    static const uint32_t LWFPGASLVS_OFST = 0xFF200000u;  // hwlib socal/hps.h ALT_LWFPGASLVS_OFST
    static const uint32_t SPAN            = 0x04000000u;  // mmap.cpp H2F_LW_REGS_SPAN

private:
    int fd_;
    volatile uint8_t* base_;
    LwBridge(const LwBridge&);
    LwBridge& operator=(const LwBridge&);
};

}}  // namespace aracno::hal
