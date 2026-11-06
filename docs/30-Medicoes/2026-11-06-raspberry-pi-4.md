---
data: 2026-11-06
spec: (S10, a escrever — [[00-Inbox/proposta-ADR-011-pi-ros-visao-na-fpga]])
---
# Raspberry Pi 4 da aranha — estado inicial

## Acesso
- Hostname `aracno-pi`, usuário `aracno`. SSH só por chave: a do PC do Luiz (`~/.ssh/id_ed25519`, `luisfpp1@al.insper.edu.br`). `sudo` sem senha.
- Wi-Fi `embarcados-IoT` (WPA2), DHCP: `172.16.30.115` em 2026-11-06. O IP pode mudar; `aracno-pi.local` resolve por mDNS a partir do PC.
- `ssh aracno@aracno-pi.local`

## Sistema (medido por ssh)
- Ubuntu Server 24.04.5 LTS, aarch64, kernel `6.8.0-1064-raspi`. `cloud-init status: done`.
- RAM 3,7 GiB (Pi 4 de 4 GB). microSD SanDisk 32 GB (SL32G): rootfs 29 G, 9 % usado. Temperatura em repouso: 35 °C.
- USB: hub VIA `2109:3431` (USB 2.0) e root hub USB 3.0 (bus 2) livre para a R200.

## Histórico do cartão
- O 1º cartão (14,8 GB) gravado pelo Imager deu `mmc0: error -110/-123` no leitor do PC: está com defeito, não reutilizar. O Pi não subia na rede com ele (LED ACT quase parado).
- O cartão atual era de outro robô (`insperbot`, Ubuntu 18.04, último uso 2023-07). Foi regravado com o Ubuntu 24.04.5 sem cópia prévia.
- Leitor de cartão do PC (RTS5129, porta USB 3-3): travou após o cartão defeituoso; `unbind/bind` falhou (`error -110`), voltou só com reboot do PC.

## RealSense R200 no Pi (2026-11-06)
- Enumera em USB 3.0 (`5000M`, porta azul), driver `uvcvideo` do kernel 6.8 **sem patch**. Nós de vídeo: `video0` IR (Y8I/GREY/Y16, 640×481…), `video2` profundidade (Z16, 628×469…), `video4` cor (YUYV até 1920×1080). `video1/3/5` são metadados.
- Só com V4L2 (sem SDK), a cor chega a 60 quadros/s, mas a profundidade vem vazia (<0,5 % válidos) e o IR fica escuro: o emissor laser fica desligado sem o SDK.
- **librealsense v1.12.1 compilada no Pi** (`pi/r200/build-librealsense.sh`). Precisou de includes para o GCC 13 e de 1 patch (ignorar nós de metadados via `device_caps`).
- `pi/r200/r200_teste.cpp` (sudo): `Intel RealSense R200 serial 2511000542 firmware 1.0.71.06`. Profundidade 480×360 a **60 quadros/s**, escala 1 mm, emissor = 1. Com a câmera a ~0,5 m de uma pessoa: 12,5 % de pixels válidos, mín 0,50 m, mediana 2,12 m. O padrão de pontos do laser aparece no IR.
- Imagem: ![[2026-11-06-r200-cor-prof-ir.png]] (cor | profundidade | IR). A câmera estava de ponta-cabeça; o tom azul da cor é troca RGB/BGR só na visualização.
- A fazer: preset de profundidade (densidade baixa com os parâmetros padrão), rodar sem `sudo` (regra udev), medir consumo e temperatura com streaming contínuo.
