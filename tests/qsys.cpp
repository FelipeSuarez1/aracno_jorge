// Contrato FPGA ↔ firmware: cada endereço usado pelo firmware (aracno/qsys.h e a fórmula dos 18 PWM
// em motor/pwm.cpp) tem que ser igual ao fpga/hps_0.h gerado pelo sopc-create-header-files.
// Mudou a FPGA sem atualizar o firmware (ou vice-versa) → este teste falha.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "aracno/qsys.h"
#include "motor/pwm.h"

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FALHA: %s\n", msg); ++fails; } } while (0)

static char text[64 * 1024];

static long base_of(const char* name) {
    char key[64];
    std::snprintf(key, sizeof key, "#define %s_BASE ", name);
    const char* p = std::strstr(text, key);
    if (!p) return -1;
    return std::strtol(p + std::strlen(key), 0, 0);
}

int main() {
    const char* paths[] = {"../fpga/hps_0.h", "fpga/hps_0.h"};
    FILE* f = 0;
    for (unsigned i = 0; i < 2 && !f; ++i) f = std::fopen(paths[i], "r");
    if (!f) { std::printf("FALHA: fpga/hps_0.h não encontrado\n"); return 1; }
    size_t n = std::fread(text, 1, sizeof text - 1, f);
    std::fclose(f);
    text[n] = 0;

    using namespace aracno;
    for (int m = 0; m < 18; ++m) {
        char name[16]; std::snprintf(name, sizeof name, "PWM%d", m);
        char msg[64]; std::snprintf(msg, sizeof msg, "PWM%d: motor::PwmBank::base_of", m);
        CHECK(base_of(name) == static_cast<long>(motor::PwmBank::base_of(m)), msg);
    }
    CHECK(base_of("PWM18") == static_cast<long>(qsys::PWM18_BASE), "PWM18");
    CHECK(base_of("PWM19") == static_cast<long>(qsys::PWM19_BASE), "PWM19");
    CHECK(base_of("LED_PIO") == static_cast<long>(qsys::LED_PIO_BASE), "LED_PIO");
    CHECK(base_of("BUTTON_PIO") == static_cast<long>(qsys::BUTTON_PIO_BASE), "BUTTON_PIO");
    CHECK(base_of("SYSID_QSYS") == static_cast<long>(qsys::SYSID_QSYS_BASE), "SYSID_QSYS");
    CHECK(base_of("PIO_LCD") == static_cast<long>(qsys::PIO_LCD_BASE), "PIO_LCD");
    CHECK(base_of("PIO_IN") == static_cast<long>(qsys::PIO_IN_BASE), "PIO_IN");
    CHECK(base_of("SPI_LCD") == static_cast<long>(qsys::SPI_LCD_BASE), "SPI_LCD");
    CHECK(base_of("CAM_CAPTURE") == static_cast<long>(qsys::CAM_CAPTURE_BASE), "CAM_CAPTURE (S09)");
    CHECK(base_of("PIO_CAM") == static_cast<long>(qsys::PIO_CAM_BASE), "PIO_CAM (S09)");
    CHECK(base_of("PIO_CAM_IN") == static_cast<long>(qsys::PIO_CAM_IN_BASE), "PIO_CAM_IN (S09)");
    {
        const char* k = "#define PIO_LCD_RESET_VALUE ";
        const char* p = std::strstr(text, k);
        CHECK(p && std::strtol(p + std::strlen(k), 0, 0) == static_cast<long>(qsys::PIO_LCD_RESET_VALUE), "PIO_LCD_RESET_VALUE");
    }
    std::printf("qsys: 29 endereços + reset do pio_lcd conferidos contra fpga/hps_0.h, %d falhas\n", fails);
    return fails ? 1 : 0;
}
