---
data: 2026-10-28
modelo: fable → opus (troca pedida pelo Luiz no meio da sessão); revisores RevisaoDisplay e RevisaoS08
specs: [[20-Specs/S04-display-rosto-balao]], [[20-Specs/S08-fpga-gpio0-display-cabeca]]
---
# Sessão 2026-10-28 — rosto da aranha no display, via FPGA

## Feito
- Driver ILI9341 (S04) escrito, revisado (REPROVADO → corrigido) e testado no PC.
- J17 (LTC) mapeado fisicamente com multímetro: pino 1 (9 V) fica na ponta do OTG, oposta ao "9V" da serigrafia.
- Jumpers não prendiam no J17 (conector com capa) → decisão: rota FPGA (S08). Quartus Prime Lite 25.1std já instalado.
- Fase 0: projeto da Terasic recompilado no Quartus 25.1 = equivalente ao de fábrica (pinos e endereços idênticos; `stand` ok na placa).
- Fase 1: spi_lcd, pio_lcd, pio_in, pwm18/19 no Platform Designer; display no GPIO_0 (JP1 pinos 1–5, 11, 12); firmware sobre o SPI da FPGA. Revisão REPROVADO → corrigido. **Rosto apareceu: quadro inteiro 131 ms, parcial 4–12 ms.**
- `fpga/build.sh` reconstrói o `.rbf` do zero a partir do repo.
- Cartão: `soc_system.rbf` = S08 (`e4a93b34…`); de fábrica guardado como `soc_system.rbf.orig` (`0c6f76f3…`).

## Aprendido
- `udisksctl power-off` desliga o leitor de cartão interno (porta USB 3-3); religar com `echo 1/0 > .../usb3-port3/disable` (root). Registrado no CLAUDE.md.
- Comando enviado pela UART durante a contagem do U-Boot interrompe o autoboot (`SOCFPGA_CYCLONE5 #`): mandar `boot`.
- Ler endereço sem escravo na ponte LW cai no pwm17 (escravo padrão do interconector): nunca escrever na faixa da S08 sem antes detectar o `.rbf`.

## Pendente para humano
- Foto do display para confirmar orientação e cores.
- Decidir sobre câmera (pergunta aberta no fim da sessão) — reabre ADR-003.
- ADR-008/009, alvo X da S05, pessoa vs. parede (Inbox).
