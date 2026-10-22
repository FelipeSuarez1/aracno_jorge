#pragma once
#include <stdint.h>

namespace aracno { namespace hal {

// Milissegundos de CLOCK_MONOTONIC (equivale a OS_GetTickCount() do app Terasic).
uint32_t now_ms();
// Microssegundos de CLOCK_MONOTONIC.
uint64_t now_us();
// Dorme até o instante absoluto (CLOCK_MONOTONIC, em us). Usado pelo laço de período fixo.
void sleep_until_us(uint64_t t_us);
void sleep_ms(uint32_t ms);

}}  // namespace aracno::hal
