---
status: aceita
data: 2026-10-22
decide: Luiz, Felipe
---
# ADR-001 — Sem placa companheira, sem troca de Linux

## Contexto
A DE0-Nano-SoC roda Yocto 2015 (SoC EDS 14.1). Reconhecimento/síntese de voz exigiriam Linux moderno ou Raspberry Pi. O corte do CPLD da Servo Motor Card desliga também o DE0 ([[40-Fontes/spider-user-manual]] cap. 5), o que obrigaria uma segunda bateria para qualquer placa extra. Existe caminho documentado de Debian na placa ([[40-Fontes/fontes-externas]]), mas é um subprojeto à parte.

## Decisão
Tudo roda no HPS (ARM Cortex-A9) da DE0-Nano-SoC, sobre a imagem Linux original, em C++ compilado estaticamente (toolchain musl em container). Nenhuma placa adicional.

## Alternativas descartadas
- Raspberry Pi como "cérebro" — duas placas, duas baterias, integração extra; só se justificava pela fala (ver ADR-002).
- Laptop via Wi-Fi — perde autonomia.
- Debian armhf na própria placa — porting de U-Boot/kernel/rootfs sem necessidade depois de ADR-002.

## Consequências
Nada de Python na placa. Toda lógica em C++11 estático. O que reabriria: necessidade de voz ou visão por câmera.
