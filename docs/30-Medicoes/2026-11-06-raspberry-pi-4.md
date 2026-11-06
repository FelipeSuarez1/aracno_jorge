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

## LiDAR SLAMTEC Mapper (2026-11-06, ligado ao PC por Ethernet)
- `GET http://192.168.11.1/service/system/product` → `{"model":"M1M1","name":"Slamtec Mapper 1 model 1","components":["cp0","lidar"]}`. Firmware `2.6.0_798_dev-sdp_vre-20190317`. Etiqueta/QR: `LH5004648`.
- Datasheet M1M1 v1.2 (SparkFun DS-15870): alcance 20 m, 7000 amostras/s, atualização de pose e mapa a 8 Hz, inclinação máxima ±3° ao mapear, velocidade máxima 1 m/s, 5 V (partida 1,4–1,6 A; em uso 450–1000 mA), 230 g, Ethernet 10/100 e Wi-Fi.
- Rede: o Mapper é servidor DHCP (`192.168.11.1`, entrega `.200`, rota padrão e DNS `114.114.114.114`). No Pi, `eth0` aceita o IP mas ignora rota/DNS (`/etc/netplan/60-eth0-mapper.yaml`).
- Protocolo SLAMWARE na porta 1445: JSON terminado em `\r\n\r\n` (`getpose`, `getlaserscan`, `getlocalizationquality`, `getsdpversion`). Implementado em `pi/mapper/mapper_scan.py`, sem o SDK da Slamtec. O formato RLE do `laser_points` foi deduzido na bancada e está documentado no script; o tamanho descomprimido confere exatamente com o declarado.
- Primeira leitura: 706 pontos por volta (≈0,0089 rad entre pontos, sentido horário), 464 válidos, distância de 0,11 a 6,85 m. Pose x=0,853 m, y=1,020 m, yaw=55,5°. ![[2026-11-06-mapper-varredura.png]]
- **Pelo Pi** (Mapper na `eth0`): o Pi recebe `192.168.11.201/24`, a rota padrão continua no `wlan0` (internet ok, ping 1.1.1.1 em 11 ms) e o ping ao Mapper leva 1,2 ms. `python3 ~/mapper_scan.py`: 713 pontos, 361 válidos, de 0,12 a 5,72 m. Pose x=0,704 m, y=1,512 m, yaw=26,2°. ![[2026-11-06-mapper-varredura-pelo-pi.png]]
- `getlocalizationquality` = 0 nas duas leituras. [INFERÊNCIA] Ele ainda não construiu mapa suficiente nesta posição; verificar movendo o Mapper pela sala.
