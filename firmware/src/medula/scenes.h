#pragma once
#include <stdint.h>

namespace aracno { namespace medula {

// Cenas de profundidade 160×120 (8 bits) da sequência F0..F19 do testbench da S11 (fpga/medula/tb_medula.v)
// e do --medula-test. Base 150 salvo indicação:
//   0: tudo 150.
//   1: retângulo x∈[60,79], y∈[40,69] em 50; ruído de 1 pixel em 50 em (10+7k, 100), k = 0..19.
//   2: cena 1 com o retângulo em 40.      3: idem em 34.      4: idem em 36.
//   5: x < 10 em 0, resto 150.
//   6: cena 1 com x < 10 em 100.
//   7: quadrado x∈[100,104], y∈[20,24] em 50.
// `id` fora de 0..7: tudo 0. Escreve N = 19200 bytes em `out`.
void make_scene(int id, uint8_t* out);

}}  // namespace aracno::medula
