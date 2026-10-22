---
data: 2026-10-22
origem: testes na placa (S03), parte B do dia 1
decide: Luiz, Felipe (quando voltar ao assunto)
---
# Watchdog de hardware: abrir /dev/watchdog reinicia o SoC — adiado

## Observado na placa (kernel 3.13.0, dw_wdt built-in, osc1 = 25,004 MHz medido pelo contador)
1. `open("/dev/watchdog")` + `WDIOC_SETTIMEOUT` → o SoC reinicia em menos de 1 s, antes do primeiro kick. Hipótese: o driver 3.13 escreve só o campo `TOP` de `TORR` e deixa `TOP_INIT = 0` → primeiro período de 2¹⁶ ciclos ≈ 2,6 ms (bug corrigido em kernels posteriores).
2. Programar `TORR`/`CR`/`CRR` direto em `0xFFD02000` via `/dev/mem` **não dá proteção**: um timer do próprio driver alimenta o cão enquanto ninguém abre o device (contador roda, nunca expira).
3. Pré-programar `TOP_INIT = TOP` por `/dev/mem` e só então abrir `/dev/watchdog` **também reiniciou** (2ª tentativa, commit desta sessão). Depois do reset a placa ficou muda na UART (precisou de ciclo de energia).

## Estado do código
`hal/watchdog.cpp` tem a versão híbrida (pré-programação + driver). `aracno` roda **sem** cão por padrão e imprime aviso; `--watchdog` liga para testar. `firmware/src/app/main.cpp` e `hal/watchdog.*` comentam o motivo.

## Caminhos para resolver (S03b)
- Ler o `dw_wdt.c` exato do kernel da imagem (fonte do kernel 3.13 socfpga da Altera; a tag `3.13.0-00298-g3c7cbb9`) para ver o que `open()` escreve e quando o timer interno para.
- Alternativa sem driver: inibir o timer do kernel escrevendo no `CR` um modo em que o restart do kernel não conte? Não existe. Alternativa real: **recompilar só o módulo**? Não é módulo (built-in).
- Alternativa robusta: watchdog **externo** barato (TPL5010 ou um ATtiny) acionando o `HPS_RST`/corte da Servo Motor Card se não receber pulso do HPS por um GPIO do PCF8574 — independe do kernel e cobre também travamento do kernel.
- Enquanto isso: a chave da Servo Motor Card é o E-stop; nenhum teste com pessoas por perto sem cão.
