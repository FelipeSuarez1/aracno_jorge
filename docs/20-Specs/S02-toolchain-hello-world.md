---
status: em-andamento
origem: [[20-Specs/S00-visao]] decisões fechadas (toolchain estática musl)
depende: [[20-Specs/S01-dia-1-uart]] (só para o critério [LAB])
---
# S02 — Toolchain cross estática e hello world na placa

## Objetivo
Qualquer pessoa com Docker compila um binário ARM estático que roda na imagem Yocto 2015 sem instalar nada na placa.

## Fora de escopo
Driver de periférico, acesso a `/dev/mem`, integração com o app `spider`.

## Fatos de entrada
- HPS = Cortex-A9 armv7-a com NEON e VFPv3, hard-float ([[40-Fontes/de0-nano-soc-user-manual]] §2.1; ABI hard-float é a do SoC EDS `[INFERÊNCIA]`).
- Toolchain musl estática evita dependência da glibc antiga ([[40-Fontes/aprofundar-v2]] §3).

## Critérios de aceite
- [PC] `docker build -t aracno-tc tools/` termina com exit 0.
- [PC] `docker run --rm -v "$PWD":/w -w /w/firmware aracno-tc make` gera `firmware/build/aracno`; `file` reporta `ELF 32-bit LSB executable, ARM, EABI5 … statically linked`.
- [PC] `make -C firmware test` compila e roda os testes de host no PC (exit 0; "0 testes" é aceitável até S03).
- [LAB] `aracno` copiado para `/home/root` via microSD ou `scp`, executado na placa: imprime `aracno ok` e a hora monotônica, exit 0.

## Verificação
- [PC]: os três comandos acima, saída colada em Evidência.
- [LAB]: `./aracno; echo $?` na UART.

## Plano
1. `tools/Dockerfile`: Debian slim + `arm-linux-musleabihf-cross` do musl.cc (versão fixada, SHA-256 verificado) ou musl-cross-make.
2. `firmware/Makefile`: `CXX=arm-linux-musleabihf-g++`, flags `-std=c++11 -O2 -static -march=armv7-a -mfpu=neon -mfloat-abi=hard -fno-exceptions -fno-rtti -Wall -Wextra`; alvo `test` compila `tests/*.cpp` com o g++ do host.
3. `firmware/src/main.cpp`: imprime versão (hash git embutido) e `clock_gettime(CLOCK_MONOTONIC)`.

## Evidência
2026-10-22, PC do Luiz (Fedora, docker). Toolchain musl.cc GCC 11.2.1, SHA-256 `11e155a0…68e58` fixado no Dockerfile.
```text
$ docker build -t aracno-tc tools/          → #9 DONE 23.4s (exit 0)
$ tools/cross-build.sh clean all
arm-linux-musleabihf-g++ -std=c++11 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -march=armv7-a -mfpu=neon -mfloat-abi=hard -Iinclude -DARACNO_GIT_REV=\"706507a\" build/main.o -o build/aracno -static -no-pie -s
firmware/build/aracno: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), statically linked, stripped   (21 736 bytes)
$ make -C firmware test
0 testes em tests/ (aceitável até S03)
testes ok
```
Gotchas: no Fedora o bind mount precisa de `:z` (SELinux) e `--user` para os arquivos não ficarem de root — já embutido em `tools/cross-build.sh`. Sem `-no-pie` o musl gera `static-pie`; forçado `-static -no-pie`. Não há `qemu-arm`/binfmt neste PC, então a execução só será provada na placa ([LAB] pendente).
