---
status: aceita
data: 2026-10-22
decide: Luiz, Felipe
---
# ADR-002 — Sem fala; o display mostra o que ela "quer falar"

## Contexto
ASR/TTS offline em pt-BR (Vosk, Piper) não rodam na imagem Yocto 2015 e o Cortex-A9 dual 925 MHz é ~metade de um Pi Zero 2 W. A placa não tem codec de áudio.

## Decisão
Nenhum reconhecimento ou síntese de voz. A aranha "fala" por um balão de texto no display, por N segundos, e depois mostra o rosto com a expressão do humor atual.

## Alternativas descartadas
- Vosk + Piper na placa — exige Debian (ADR-001).
- Frases pré-sintetizadas em WAV + alto-falante USB — possível, mas adiciona áudio USB e CPU; fica como ideia futura no Inbox se sobrar tempo.

## Consequências
Sem microfone de voz. Opcional: microfone analógico no ADC para reagir a barulho por amplitude (não é fala). Frases vivem numa tabela no código.
