---
data: 2026-10-22
modelo: fable (integração e núcleo); subagentes: SimPolitica (S05), FaceRender (S04), RevisaoS03 (opus, revisão)
specs: [[20-Specs/S03-firmware-base-e-seguranca]], [[20-Specs/S04-display-rosto-balao]], [[20-Specs/S05-simulador-e-politica]]
---
# Sessão 2026-10-22 (noite) — S03 núcleo, S04/S05 periferia, primeiros testes na placa

## Feito
- Commits `5fee63a` → `59c99be` (12). S03 núcleo + revisão de segurança (6 bloqueios corrigidos); S04 lado PC (rosto, fonte, testes pixel-exatos); S05 (simulador, Q-learning, export, `--policy` no laço).
- Na placa: `spider` original desativado no `inittab` (backup em `/etc/inittab.orig`); `aracno` gravado por UART (push em pedaços com md5, ~1 min para 124 kB); laço de 50 Hz medido: p50 20 000 µs / p99 20 011 µs; `M_STAND` completo com STATUS da FPGA.
- Três bugs só visíveis no hardware corrigidos: `MCL_FUTURE` + `/dev/mem` reinicia o SoC; tombo por tranco de manuseio; demo contava `STAND` abortado como concluído.

## Verificado
- `make -C firmware test`: face, log, policy, pwm/spider, safety (35 casos) — ok. Cross estático 124 kB.
- Logs reais convertidos por `tools/log2csv.py` (CSV em `30-Medicoes/`).

## Pendente para humano
- **Watchdog**: três abordagens falharam na placa; adiado a pedido do Luiz. Nota com caminhos em `00-Inbox/watchdog-dw-wdt-reinicia.md`. Até lá: chave da SMK = E-stop, nada com gente por perto.
- A placa ficou muda após o último reset: desligar/ligar a fonte 5 V antes do próximo teste.
- Próximo teste físico: SMK ligada, aranha no chão com 2 m livres, `./aracno --demo stand --sensors 0x20 --speed 20`.
- Decisões no Inbox: pessoa vs. parede (S05), ADR-008, ADR-009, alvo X.

## Aprendido
- Nunca `mlockall(MCL_FUTURE)` com `/dev/mem` mapeado nesta placa.
- musl: `sched_setscheduler` devolve ENOSYS — usar `syscall(SYS_sched_setscheduler)`.
- `dw_wdt` 3.13: driver alimenta o cão sozinho enquanto o device está fechado; abrir reinicia (TOP_INIT).
- BusyBox 1.20: sem `od -A`, sem `devmem`; `hexdump -v -e` funciona para puxar logs pela UART.
- A tty da UART mistura `dmesg` (dongle BT em *overcurrent*) com a saída do programa; filtrar `dwc2|usb`.
