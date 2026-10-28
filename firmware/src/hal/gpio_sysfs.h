#pragma once

namespace aracno { namespace hal {

// GPIO do HPS via /sys/class/gpio (kernel 3.13: gpiochip171/198/227 — dia 1).
// Uso no projeto: gpio209 = HPS_LTC_GPIO = seletor do mux TS3A5018 do header LTC;
// 0 → SPI1 nos pinos 4/5/6/7 (docs/40-Fontes/hardware.md). Sem pull-up na placa (R112 DNI):
// tem que ser forçado por software antes de abrir /dev/spidev1.0.
static const int GPIO_LTC_MUX = 209;

// Exporta (se preciso), põe como saída e escreve o valor. false se o sysfs recusar.
bool gpio_set_output(int gpio, bool value);

}}  // namespace aracno::hal
