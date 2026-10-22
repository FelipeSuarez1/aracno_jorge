---
data: 2026-10-22
modelo: fable (planejamento, leitura de hardware, escrita de specs e tools)
specs: [[20-Specs/S01-dia-1-uart]], [[20-Specs/S02-toolchain-hello-world]], [[20-Specs/S03-firmware-base-e-seguranca]], [[20-Specs/S04-display-rosto-balao]], [[20-Specs/S05-simulador-e-politica]], [[20-Specs/S06-cabeca-pan-tilt]]
---
# Sessão 2026-10-22 (tarde) — dia 1 completo, toolchain validada na placa, specs S03–S06

## Feito
- Commits: `19d0b2d` dia 1 parte A (microSD lida no PC) · `b64a3b7` parte C (Spider CD importado) · `57d6a5b` parte B (UART ao vivo) · `c758b3f` `tools/uart.py` + S02 na placa · `46e1470` CLAUDE.md · `5bdb52b` specs S03–S06 + Compras · este commit: S04/S06 ajustadas ao display e servos do lab.
- Backup da microSD (`~/dev/aracno_backup/`, SHA-256 `44254bf2…28c6d`); cartão = Linux Image 2.0.
- Fonte do Spider CD em `40-Fontes/spider-cd/`; fatos extraídos em [[30-Medicoes/dia-1]]: registradores PWM (incl. `ABORT`), faixa −90…+90°, limiares do CPLD (5,9 V/2 s; 65 W/0,2 s), INA230 cfg `0x4127`, comandos extras `ATST/ATVR=?/ATJS=`.
- UART: kernel 3.13.0, `/dev/i2c-1`, `/dev/spidev1.0`, `/dev/watchdog` confirmados; I2C1 sem pull-up; `PREEMPT_NONE`; dongle BT com avisos de DMA/overcurrent.
- `tools/uart.py` (run/push/boot) — único caminho para a placa (sem rede, sem base64).
- Display escolhido: Adafruit ILI9341 2,8" com touch (lab). Pan/tilt com os 2 MG996R reserva.

## Verificado
- `tools/uart.py push firmware/build/aracno /home/root/aracno` → md5 ok; `./aracno` → `aracno ok rev=706507a monotonic=…`, exit 0.
- `make -C firmware test` → 0 testes, exit 0.

## Pendente para humano
- Aceitar ADR-008 (papéis) e ADR-009 (rota HPS) — mudar `status:` e mover ADR-009 do Inbox para `10-Decisoes/`.
- Aprovar specs S03–S06 (rascunho). S05 exige fixar o alvo X antes do primeiro treino.
- Lista para o professor: [[Compras]] (prioridade: VL53L1X ×2, BH1750 ×2, PIR, PCF8574, PCA9685, 4,7 kΩ, suporte pan/tilt).
- Conferir no display: jumpers IM1–3 e chip de touch (STMPE610 ou TSC2007).

## Aprendido
- Os dois Mini-B da DE0: um é USB-Blaster II (`09fb:6810`), o outro UART (`0403:6001`). O FT232R re-enumera quando a placa reinicia.
- Sem cartão, a placa fica muda na UART (RXD pisca, TXD não) — checar `lsblk` antes de culpar o cabo.
- Heredoc pela serial corrompe (histórico `!` e buffer da tty); envio linha a linha com eco confirmado funciona.
- `i2cdetect -y N` falha no DesignWare; usar `-r`.
- Proposta para o CLAUDE.md (já aplicada): comando `tools/uart.py` e "Servo Motor Card OFF em teste de software".
