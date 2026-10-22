# aracno_jorge

Aranha robótica autônoma com "cabeça humana": Terasic Spider (hexápode, 18× MG996R) controlado por uma DE0-Nano-SoC, com cabeça pan/tilt, display com rosto e balão de texto, sensores de luz/distância/presença e uma política de decisão treinada no PC e executada em C++ no ARM da placa.

Projeto de Luiz e Felipe (Engenharia da Computação). Todo o código é escrito por IA; os autores decidem, medem no laboratório e revisam.

## Como navegar
- `docs/` — vault Obsidian: abra esta pasta no Obsidian. Comece por `docs/Home.md`.
- `CLAUDE.md` — regras que a IA lê em toda sessão.
- `firmware/` — C++ do HPS (extensão do app `spider` da Terasic).
- `sim/` — simulador 2D e treino da política.
- `tools/` — toolchain cross (Docker), scripts de flash e de logs.
- `tests/` — testes de host que rodam no PC.

## Build rápido
```sh
tools/cross-build.sh         # docker build + make; imprime `file firmware/build/aracno`
make -C firmware test        # testes de host no PC
```
