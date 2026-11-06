#!/usr/bin/env python3
"""Lê pose e uma varredura do SLAMTEC Mapper M1M1 pelo protocolo TCP do SLAMWARE (porta 1445, JSON + "\r\n\r\n").

Formato de laser_points, deduzido na bancada (2026-11-06), sem documentação pública:
  base64 → "RLE" + sentinela(1) + sentinela_alternativa(1) + tamanho_descomprimido(uint32 LE) + dados
  dados: byte comum = literal; [sentinela, n>0, v] = v repetido n vezes; [sentinela, 0, s] = passa a usar s como sentinela.
  descomprimido: pontos de 12 bytes = distância float32 (m), ângulo float32 (rad), válido uint8, 3 bytes de enchimento.
Uso: mapper_scan.py [host] [png]   (padrão 192.168.11.1, sem imagem)
"""
import base64, json, math, socket, struct, sys


class Mapper:
    def __init__(self, host="192.168.11.1", port=1445):
        self.s = socket.create_connection((host, port), timeout=5)
        self.rid = 0

    def ask(self, command, args=None):
        self.rid += 1
        self.s.sendall((json.dumps({"command": command, "args": args, "request_id": self.rid}) + "\r\n\r\n").encode())
        buf = b""
        while b"\r\n\r\n" not in buf:
            chunk = self.s.recv(65536)
            if not chunk:
                raise ConnectionError("Mapper fechou a conexão")
            buf += chunk
        return json.loads(buf.split(b"\r\n\r\n")[0])["result"]

    def scan(self):
        return decode_points(self.ask("getlaserscan")["laser_points"])


def rle_decode(raw):
    if raw[:3] != b"RLE":
        raise ValueError("cabeçalho RLE ausente")
    sentinel = raw[3]
    size = struct.unpack_from("<I", raw, 5)[0]
    out = bytearray()
    i, data = 0, raw[9:]
    while i < len(data):
        c = data[i]
        i += 1
        if c != sentinel:
            out.append(c)
            continue
        n, v = data[i], data[i + 1]
        i += 2
        if n == 0:
            sentinel = v
        else:
            out += bytes([v]) * n
    if len(out) != size:
        raise ValueError(f"RLE: {len(out)} bytes, esperado {size}")
    return bytes(out)


def decode_points(b64):
    data = rle_decode(base64.b64decode(b64))
    return [struct.unpack_from("<ffB", data, k) for k in range(0, len(data), 12)]


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "192.168.11.1"
    m = Mapper(host)
    pose = m.ask("getpose")
    print("pose: x=%.3f m y=%.3f m yaw=%.1f°  qualidade de localização=%s" %
          (pose["x"], pose["y"], math.degrees(pose["yaw"]), m.ask("getlocalizationquality")))
    pts = m.scan()
    ok = [p for p in pts if p[2]]
    d = sorted(p[0] for p in ok)
    print("varredura: %d pontos, %d válidos; distância mín %.2f m, mediana %.2f m, máx %.2f m" %
          (len(pts), len(ok), d[0], d[len(d) // 2], d[-1]))
    front = [p[0] for p in ok if abs(math.atan2(math.sin(p[1]), math.cos(p[1]))) < math.radians(10)]
    if front:
        print("à frente (±10°): mais perto a %.2f m" % min(front))
    if len(sys.argv) > 2:
        import cv2, numpy as np
        im = np.zeros((500, 500, 3), np.uint8); c, sc = 250, 50
        for k in range(1, 6):
            cv2.circle(im, (c, c), k * sc, (60, 60, 60), 1)
        for dist, a, _ in ok:
            x, y = int(c + dist * math.cos(a) * sc), int(c - dist * math.sin(a) * sc)
            if 0 <= x < 500 and 0 <= y < 500:
                cv2.circle(im, (x, y), 2, (0, 255, 255), -1)
        cv2.arrowedLine(im, (c, c), (c + 30, c), (0, 0, 255), 2)
        cv2.putText(im, "Mapper M1M1 (frente ->), aneis = 1 m", (10, 20), 0, 0.5, (200, 200, 200), 1)
        cv2.imwrite(sys.argv[2], im)


if __name__ == "__main__":
    main()
