#!/usr/bin/env python3
"""Converte o log binário do aracno (firmware/include/aracno/log.h, versão 1) em CSV.

Uso: tools/log2csv.py aracno.log [saida.csv]   (sem saída → stdout)
Imprime no stderr um resumo: registros válidos/corrompidos, p50/p99/max do período,
contagem por motivo de segurança e por ação permitida.
"""
import struct, sys, zlib, collections

MAGIC = 0x4A52414A
N_SENSORS = 7
# struct LogRec (little-endian, campos na ordem do header; Obs é empacotada pelo compilador
# com alinhamento natural — por isso o layout abaixo inclui os paddings do ARM EABI).
OBS_FMT = '<I HH HH BB hhh I ' + 'H' * N_SENSORS + ' B'   # t_ms, lux_l, lux_r, dist_f, dist_fl, pir, estop, acc x y z, t_since, ages[7], installed
# Obs tem alinhamento 4: tamanho = 4+2+2+2+2+1+1+2*3+4+14+1 = 39 → arredonda para 40.
OBS_SIZE = 40
REC_FMT = '<IHH' + '%ds' % OBS_SIZE + 'BB BB B B B B I I'
# magic, version, size, obs, desired(action,speed), allowed(action,speed), reason, abort, motion, step, period_us, crc
REC_SIZE = struct.calcsize(REC_FMT)

ACTIONS = ['STOP', 'FORWARD', 'BACKWARD', 'TURN_LEFT', 'TURN_RIGHT', 'CROUCH', 'LUNGE', 'HEAD_LEFT', 'HEAD_RIGHT']
REASONS = ['ok', 'estop', 'travado', 'obsoleto', 'tombo', 'borda', 'obstaculo']
MOTIONS = ['NONE', 'STAND', 'FORWARD', 'BACKWARD', 'ROTATE_L', 'ROTATE_R', 'TILT_F', 'TILT_B', 'TILT_L', 'TILT_R', 'TILT_N', 'CROUCH', 'LUNGE', 'SLEEP_POSE']


def name(table, i):
    return table[i] if i < len(table) else str(i)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    data = open(sys.argv[1], 'rb').read()
    out = open(sys.argv[2], 'w') if len(sys.argv) > 2 else sys.stdout
    hdr = ['t_ms', 'lux_l', 'lux_r', 'dist_front_mm', 'dist_floor_mm', 'pir', 'estop', 'acc_x_mg', 'acc_y_mg', 'acc_z_mg',
           't_since_ms'] + ['age_%d' % i for i in range(N_SENSORS)] + ['installed', 'desired', 'speed_desired', 'allowed',
           'speed_allowed', 'reason', 'abort', 'motion', 'step', 'period_us']
    out.write(','.join(hdr) + '\n')
    ok = bad = 0
    periods = []
    reasons = collections.Counter()
    allowed = collections.Counter()
    off = 0
    while off + REC_SIZE <= len(data):
        if struct.unpack_from('<I', data, off)[0] != MAGIC:
            off += 1
            bad += 1
            continue
        rec = data[off:off + REC_SIZE]
        magic, ver, size, obs, da, ds, aa, as_, reason, abort, motion, step, period, crc = struct.unpack(REC_FMT, rec)
        if size != REC_SIZE or zlib.crc32(rec[:-4]) & 0xFFFFFFFF != crc:
            off += 4
            bad += 1
            continue
        o = struct.unpack_from(OBS_FMT, obs)
        row = list(o[:11]) + list(o[11:11 + N_SENSORS]) + [o[11 + N_SENSORS], name(ACTIONS, da), ds, name(ACTIONS, aa), as_,
                                                             name(REASONS, reason), abort, name(MOTIONS, motion), step, period]
        out.write(','.join(str(x) for x in row) + '\n')
        ok += 1
        periods.append(period)
        reasons[name(REASONS, reason)] += 1
        allowed[name(ACTIONS, aa)] += 1
        off += REC_SIZE
    if periods:
        p = sorted(periods)
        p50, p99, mx = p[len(p) // 2], p[int(len(p) * 0.99)], p[-1]
    else:
        p50 = p99 = mx = 0
    print('registros ok=%d corrompidos=%d periodo p50=%d us p99=%d us max=%d us' % (ok, bad, p50, p99, mx), file=sys.stderr)
    print('motivos:', dict(reasons), file=sys.stderr)
    print('acoes permitidas:', dict(allowed), file=sys.stderr)


if __name__ == '__main__':
    main()
