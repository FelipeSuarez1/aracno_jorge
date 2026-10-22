// Registro binário gravado a cada ciclo do laço (S03). Lido por tools/log2csv.py.
// Little-endian, layout do ARM EABI (Obs tem 1 byte de padding no fim; o gravador zera o registro
// antes de preencher). Mudou → mudar tools/log2csv.py e LOG_VERSION.
#pragma once
#include <stdint.h>
#include "aracno/types.h"

namespace aracno {

static const uint32_t LOG_MAGIC   = 0x4A52414Au;  // "JARJ" — início de cada registro
static const uint16_t LOG_VERSION = 1;

struct LogRec {
    uint32_t magic;          // LOG_MAGIC
    uint16_t version;        // LOG_VERSION
    uint16_t size;           // sizeof(LogRec)
    Obs      obs;
    Cmd      desired;        // o que o cérebro pediu
    Cmd      allowed;        // o que a segurança liberou
    uint8_t  reason;         // safety::Reason
    uint8_t  abort;          // 1 = servos soltos neste ciclo
    uint8_t  motion;         // motor::Motion em curso
    uint8_t  step;           // índice do passo
    uint32_t period_us;      // duração real do ciclo anterior
    uint32_t crc;            // CRC-32 (IEEE) de tudo acima
};

uint32_t crc32_ieee(const void* data, uint32_t len);

}  // namespace aracno
