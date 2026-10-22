#include "sense/vl53l1x.h"

namespace aracno { namespace sense {

// Registradores (ST ULD vl53l1x_api.h / vl53l1_platform.h).
enum {
    REG_I2C_SLAVE_ADDRESS         = 0x0001,
    REG_GPIO_HV_MUX_CTRL          = 0x0030,
    REG_GPIO_TIO_HV_STATUS        = 0x0031,
    REG_SYSTEM_INTERRUPT_CLEAR    = 0x0086,
    REG_SYSTEM_MODE_START         = 0x0087,
    REG_RESULT_RANGE_STATUS       = 0x0089,
    REG_RESULT_FINAL_RANGE_MM     = 0x0096,
    REG_FIRMWARE_SYSTEM_STATUS    = 0x00E5,
    REG_IDENTIFICATION_MODEL_ID   = 0x010F
};

// VL51L1X_DEFAULT_CONFIGURATION (ST ULD), endereços 0x2D..0x87, 91 bytes.
static const uint8_t DEFAULT_CFG[91] = {
    0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x02, 0x08, 0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0xff, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x0b, 0x00, 0x00, 0x02, 0x0a, 0x21,
    0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x38, 0xff, 0x01, 0x00, 0x08, 0x00,
    0x00, 0x01, 0xcc, 0x0f, 0x01, 0xf1, 0x0d, 0x01, 0x68, 0x00, 0x80, 0x08, 0xb8, 0x00, 0x00, 0x00,
    0x00, 0x0f, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0f, 0x0d, 0x0e, 0x0e, 0x00,
    0x00, 0x02, 0xc7, 0xff, 0x9B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00
};

// Mapa ULD de RESULT_RANGE_STATUS (5 bits) → status da API: 9 = range válido.
static const uint8_t STATUS_RTN[24] = {255, 255, 255, 5, 2, 4, 1, 7, 3, 0, 255, 255, 9, 13, 255, 255,
                                       255, 255, 10, 255, 255, 255, 14, 255};

Vl53l1x::Vl53l1x() : bus_(0), addr_(DEFAULT_ADDR), ok_(false), status_(255), pol_(1) {}

bool Vl53l1x::init(hal::I2cBus* bus, uint8_t addr) {
    bus_ = bus;
    addr_ = addr;
    ok_ = false;
    uint8_t id = 0;
    if (!bus_->read_reg16(addr_, REG_IDENTIFICATION_MODEL_ID, &id, 1) || id != 0xEA) return false;
    // Espera boot (bit0 de FIRMWARE_SYSTEM_STATUS) — até ~50 tentativas.
    uint8_t boot = 0;
    for (int i = 0; i < 50 && !(boot & 1); ++i) bus_->read_reg16(addr_, REG_FIRMWARE_SYSTEM_STATUS, &boot, 1);
    if (!(boot & 1)) return false;
    for (uint16_t i = 0; i < sizeof(DEFAULT_CFG); ++i)
        if (!bus_->write_reg16(addr_, static_cast<uint16_t>(0x2D + i), &DEFAULT_CFG[i], 1)) return false;
    // Polaridade da interrupção (ULD GetInterruptPolarity): bit4 de GPIO_HV_MUX_CTRL, invertido.
    uint8_t mux = 0;
    if (!bus_->read_reg16(addr_, REG_GPIO_HV_MUX_CTRL, &mux, 1)) return false;
    pol_ = static_cast<uint8_t>(((mux & 0x10) >> 4) ^ 1);
    // Ranging contínuo (ULD StartRanging = 0x40). Intervalo entre amostras vem da configuração
    // padrão (0x6C..0x6F) — [A MEDIR] na bancada: precisa ser < stale_ms (200 ms).
    uint8_t v = 0x40;
    if (!bus_->write_reg16(addr_, REG_SYSTEM_MODE_START, &v, 1)) return false;
    ok_ = true;
    return true;
}

bool Vl53l1x::set_address(uint8_t new_addr) {
    if (!bus_) return false;
    if (!bus_->write_reg16(addr_, REG_I2C_SLAVE_ADDRESS, &new_addr, 1)) return false;
    addr_ = new_addr;
    return true;
}

bool Vl53l1x::poll(uint16_t* mm) {
    if (!ok_) return false;
    uint8_t tio = 0;
    if (!bus_->read_reg16(addr_, REG_GPIO_TIO_HV_STATUS, &tio, 1)) return false;
    if ((tio & 1) != pol_) return false;                          // sem amostra nova
    uint8_t st = 0, d[2] = {0, 0};
    // Qualquer falha aqui = sem amostra (não carimbar idade com dado incompleto).
    if (!bus_->read_reg16(addr_, REG_RESULT_RANGE_STATUS, &st, 1)) return false;
    if (!bus_->read_reg16(addr_, REG_RESULT_FINAL_RANGE_MM, d, 2)) return false;
    uint8_t clr = 0x01;
    if (!bus_->write_reg16(addr_, REG_SYSTEM_INTERRUPT_CLEAR, &clr, 1)) return false;
    st &= 0x1F;
    status_ = st < 24 ? STATUS_RTN[st] : 255;
    *mm = (status_ == 0) ? static_cast<uint16_t>((d[0] << 8) | d[1]) : 0;
    return true;
}

}}  // namespace aracno::sense
