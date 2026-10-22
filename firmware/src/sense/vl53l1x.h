#pragma once
#include <stdint.h>
#include "hal/i2c.h"

namespace aracno { namespace sense {

// VL53L1X em modo contínuo, driver mínimo transcrito do ST "VL53L1X ULD" (API
// ultra-lite, tabela VL51L1X_DEFAULT_CONFIGURATION 0x2D..0x87). [INFERÊNCIA] até o
// teste de bancada: conferir os 91 bytes contra o pacote STSW-IMG009 baixado.
class Vl53l1x {
public:
    static const uint8_t DEFAULT_ADDR = 0x29;

    Vl53l1x();
    // Verifica MODEL_ID (0xEA) e carrega a configuração padrão; inicia ranging contínuo.
    bool init(hal::I2cBus* bus, uint8_t addr);
    // Muda o endereço I2C do sensor (reg 0x0001); usar com o outro sensor em XSHUT=0.
    bool set_address(uint8_t new_addr);
    // Se há amostra nova: devolve true e preenche mm (0 se status inválido), limpa a interrupção.
    bool poll(uint16_t* mm);
    bool ok() const { return ok_; }
    uint8_t addr() const { return addr_; }
    uint8_t last_status() const { return status_; }

private:
    hal::I2cBus* bus_;
    uint8_t addr_;
    bool ok_;
    uint8_t status_;
    uint8_t pol_;   // polaridade da interrupção (lida uma vez em init)
};

}}  // namespace aracno::sense
