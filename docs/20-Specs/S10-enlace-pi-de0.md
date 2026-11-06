---
status: rascunho
origem: [[10-Decisoes/ADR-011-pi-ros-visao-na-fpga]]
---
# S10 — Enlace Pi ↔ DE0: profundidade da R200 para a FPGA e resultado de volta

## Objetivo
O Raspberry Pi 4 manda à DE0, por um cabo Ethernet direto, o mapa de profundidade da RealSense R200 reduzido a 160×120 de 8 bits; a DE0 devolve ao Pi o resultado da "medula" (S11) e o comportamento atual. Sem o Pi, a aranha volta a funcionar só com a FSM.

## Fora de escopo
- A implementação do enlace neste plano: esta spec é só **rascunho**; o plano "medula-fpga" implementa a S11 e deixa o enlace para depois.
- O processamento na FPGA (S11), a política de comportamento e a reação das pernas ao reflexo.
- ROS 2 no Pi, o driver da R200 e o preset de profundidade (ficam no Pi; ver [[10-Decisoes/ADR-011-pi-ros-visao-na-fpga]]).
- O LiDAR Mapper (continua na `eth0` do Pi, em outra rede).
- Cor, IR, criptografia/autenticação do enlace e retransmissão de quadros perdidos.

## Fatos de entrada
- R200 no Pi: enumera em USB 3.0; profundidade 480×360 Z16 a 60 quadros/s, escala 1 mm, emissor ligado só com o SDK (librealsense v1.12.1 compilada no Pi). Fonte: [[30-Medicoes/2026-11-06-raspberry-pi-4]], seção "RealSense R200 no Pi".
- Pi: hostname `aracno-pi`, Ubuntu Server 24.04.5 (netplan). A `eth0` está ocupada pelo LiDAR Mapper (rede `192.168.11.0/24`; `/etc/netplan/60-eth0-mapper.yaml`, a `eth0` ignora rota/DNS). Fonte: [[30-Medicoes/2026-11-06-raspberry-pi-4]], seção "LiDAR SLAMTEC Mapper". Por isso o cabo da DE0 usa um adaptador USB-Ethernet no Pi (`eth1`). `[INFERÊNCIA]` o nome `eth1` depende do adaptador: confirmar com `ip link` no lab.
- DE0: o boot mostra `eth0` e `sshd` (Linux da Terasic). `[A VERIFICAR no lab]` a DE0 sobe a `eth0` com IP fixo e recebe/envia UDP.
- Quadro de profundidade reduzido: 160×120 = 19 200 B, o mesmo tamanho e empacotamento do FRAME do `medula` (S11: 4 800 palavras, pixel p no byte p%4); ver [[20-Specs/S11-medula-fpga]], Contrato.
- Resultado: os campos de `medula::Result` definidos na S11 (target, count, cx, cy, min_depth, mean_depth, bbox, reflex, HEAD_TARGET).

## Design
### Físico
- Cabo direto do Pi à DE0 por um adaptador USB-Ethernet no Pi (`eth1`), porque a `eth0` está com o Mapper.
- IPs fixos, sem gateway: Pi `10.0.0.1/24` (`eth1`), DE0 `10.0.0.2/24` (`eth0`).

### Quadro Pi → DE0: UDP, porta 5600
Cabeçalho de **16 bytes**, little-endian:

|Offset|Campo|Tipo|Conteúdo|
|---|---|---|---|
|0|magic|4 bytes|`AJF1`|
|4|`frame_id`|u32|número do quadro, cresce a cada quadro|
|8|`chunk`|u16|índice do pedaço, de 0|
|10|`nchunks`|u16|total de pedaços do quadro|
|12|`len`|u16|bytes de payload neste datagrama|
|14|`flags`|u16|bit0 `BG_RESET`: o corpo se mexeu (a DE0 manda BG_RESET no GO deste quadro); demais bits 0|

- Payload ≤ 1400 B por datagrama. Um quadro tem 19 200 B = **14 pedaços** (13 de 1400 B e 1 de 1000 B).
- A DE0 descarta quadro incompleto; um `frame_id` novo descarta o quadro anterior em montagem.

### Conversão de profundidade (no Pi)
- Entrada: R200 480×360 Z16, em mm.
- Blocos de 3×3 pixels (480/3 = 160, 360/3 = 120); em cada bloco, o **menor valor ≠ 0**.
- `depth8 = 0` se o bloco não tiver medida; senão `depth8 = min(255, max(1, mm >> 4))` (16 mm por unidade, alcance até 4,08 m).
- Resultado 160×120, linha a linha, pixel p = y·160 + x. Esse é o corpo (19 200 B) dos pedaços, na ordem de `chunk`.

### Resultado DE0 → Pi: UDP, porta 5601
- Magic `AJR1`, `frame_id u32` (o do quadro a que o resultado se refere) e depois os campos de `medula::Result` da S11: target, count, cx, cy, min_depth, mean_depth, bbox (x0, y0, x1, y1), reflex, pan alvo e tilt alvo (= HEAD_TARGET: pan_t e tilt_t efetivo), seguidos de um `u8` com o enum do comportamento atual da FSM.
- Little-endian, sem preenchimento, nessa ordem, com os tipos de `medula::Result` (`bool`/`u8` em 1 byte; `count`, pan e tilt em `u16`). `[A DEFINIR]` o layout byte a byte e os valores do enum, ao implementar o enlace (esta spec é rascunho).

### Falha do Pi
- Sem quadro novo por tempo limite, a DE0 desliga `FOLLOW_EN` e `REFLEX_EN` do `medula` e a política continua só com a FSM. `[A DEFINIR]` o tempo limite (o critério [LAB] abaixo exige < 1 s).

## Critérios de aceite
- [PC] A conversão de profundidade do Pi dá `depth8` conforme a fórmula acima: bloco sem medida → 0; 1 mm → 1; 4 080 mm → 255; 5 000 mm → 255; menor valor ≠ 0 do bloco; saída 160×120 (19 200 B).
- [PC] Serialização/montagem: cabeçalho de 16 bytes com `AJF1`; 14 pedaços de um quadro de 19 200 B remontam o quadro original; quadro com pedaço faltando é descartado; `frame_id` novo descarta o anterior.
- [LAB] ≥ 10 quadros/s ponta a ponta (câmera → Pi → DE0 → resultado de volta ao Pi).
- [LAB] Perda < 5 % em 60 s (quadros enviados × resultados recebidos).
- [LAB] Sem o Pi (cabo ou processo desligado), a aranha volta à FSM em < 1 s.

## Verificação
- [PC] Testes de host (Pi: Python, DE0: `make -C firmware test`) com um quadro de profundidade sintético e datagramas gerados; formato do log: uma linha `OK`/`FALHOU` por caso.
- [LAB] Pi com a R200 e a DE0 ligados pelo cabo (`ping 10.0.0.2` primeiro). Contar quadros enviados e resultados recebidos por 60 s e tirar o fps médio; log colado na Evidência. Para a queda do Pi, desligar o processo do Pi (e depois o cabo) e medir com o log da DE0 o tempo até o comportamento voltar à FSM.

## Riscos
- O adaptador USB-Ethernet pode não ser reconhecido no Pi (nome da interface, driver); trocar de adaptador ou usar outro IP não muda o protocolo.
- UDP perde e reordena datagramas; o descarte de quadro incompleto aceita isso, o limite é o critério de perda < 5 %.
- Carga do ARM da DE0 ao receber 19 200 B por quadro e escrever 4 800 palavras no FRAME do `medula`: pode limitar o fps (`[INFERÊNCIA]`).
- A rede da DE0 pode estar desativada ou com outro IP no Linux da Terasic.

## Plano
1. `periferia` — Pi: conversão Z16 → depth8 160×120 e remetente UDP 5600 (com `BG_RESET` quando o corpo se mexe); teste de host.
2. `periferia` — DE0: receptor UDP 5600 (montagem de pedaços, descarte de quadro incompleto), `upload`/`start` no driver `medula` (S11) e resultado UDP 5601; testes de host.
3. `núcleo` — Integração no laço do firmware: comportamento sem o Pi (queda < 1 s → FSM, `FOLLOW_EN`/`REFLEX_EN` desligados) e uso do resultado pela política (camada de segurança).
4. Revisão `reviewer` (opus) dos passos 2 e 3.
5. [LAB] IPs fixos, `ping`, fps, perda e queda do Pi.

## Evidência
