#!/usr/bin/env python3
"""Fala com o shell root da DE0-Nano-SoC pela UART (FT232R, 115200 8N1).

A placa não tem rede nem base64; o envio usa uuencode + perl (ambos existem na
imagem Yocto original) com confirmação de eco linha a linha.

Uso:
  tools/uart.py run  "uname -a; ls /dev/i2c*"      # roda e imprime a saída
  tools/uart.py push firmware/build/aracno /home/root/aracno   # copia e confere md5
  tools/uart.py boot [segundos]                     # captura o log de boot
Opções: --dev /dev/ttyUSB0 (padrão: primeiro /dev/ttyUSB*), --log arquivo (anexa tudo).
Pré-requisito: usuário no grupo plugdev/dialout; placa com login automático de root.
"""
import argparse, binascii, fcntl, glob, hashlib, os, select, struct, sys, termios, time

PROMPT = 'root@socfpga:~# '


class Uart:
    def __init__(self, dev, log=None):
        self.fd = os.open(dev, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0] = 0; a[1] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL; a[3] = 0
        a[4] = a[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        fcntl.ioctl(self.fd, 0x5416, struct.pack('I', 0x002 | 0x004))  # TIOCMBIS DTR|RTS
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.log = open(log, 'a') if log else None

    def read(self, seconds):
        out = b''; end = time.time() + seconds
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.1)
            if r:
                try: out += os.read(self.fd, 4096)
                except BlockingIOError: pass
        s = out.decode(errors='replace')
        if self.log: self.log.write(s); self.log.flush()
        return s

    def run(self, cmd, timeout=20):
        """Envia um comando e devolve a saída (sem o eco nem o prompt final)."""
        termios.tcflush(self.fd, termios.TCIFLUSH)
        os.write(self.fd, (cmd + '\n').encode())
        out = ''; end = time.time() + timeout
        while time.time() < end:
            out += self.read(0.3)
            if out.rstrip().endswith(PROMPT.strip()):
                break
        body = out.replace('\r', '')
        body = body[:body.rfind(PROMPT.strip())] if PROMPT.strip() in body else body
        return body.split('\n', 1)[1] if '\n' in body else ''

    def push(self, local, remote, chunk_lines=150):
        """Envia `local` para `remote` em pedaços uuencoded verificados por md5sum na placa."""
        data = open(local, 'rb').read()
        lines = [binascii.b2a_uu(data[i:i + 45]).decode() for i in range(0, len(data), 45)]
        chunks = [lines[i:i + chunk_lines] for i in range(0, len(lines), chunk_lines)]
        self.run('set +H; rm -f /tmp/push.*', 5)
        for ci, chunk in enumerate(chunks):
            text = ''.join(chunk)
            want = hashlib.md5(text.encode()).hexdigest()
            for attempt in range(4):
                termios.tcflush(self.fd, termios.TCIFLUSH)
                os.write(self.fd, ('cat > /tmp/push.%03d\n' % ci).encode()); time.sleep(0.2); self.read(0.1)
                for l in chunk:
                    os.write(self.fd, l.encode()); time.sleep(0.012)   # ~5 kB/s, metade da UART com eco
                    if self.log is None: self.read(0)
                os.write(self.fd, b'\x04'); time.sleep(0.3); self.read(0.3)
                got = self.run('md5sum /tmp/push.%03d' % ci, 10).split()
                if got and got[0] == want: break
                sys.stderr.write('pedaço %d/%d: md5 difere, tentativa %d\n' % (ci + 1, len(chunks), attempt + 2))
            else:
                raise RuntimeError('pedaço %d não confirmou após 4 tentativas' % ci)
            if ci % 5 == 4 or ci == len(chunks) - 1:
                sys.stderr.write('%d/%d pedaços ok\n' % (ci + 1, len(chunks)))
        self.run("cat /tmp/push.0* > /tmp/push.uu; perl -e 'open F,\"</tmp/push.uu\"; open O,\">%s\"; binmode O; "
                 "while(<F>){print O unpack(\"u\",$_)} close O'; chmod +x %s; rm -f /tmp/push.0*" % (remote, remote), 20)
        got = self.run('md5sum ' + remote, 10).split()
        want = hashlib.md5(data).hexdigest()
        if not got or got[0] != want:
            raise RuntimeError('md5 difere: placa=%s esperado=%s' % (got[:1], want))
        return len(data), want


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--dev'); p.add_argument('--log')
    sub = p.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run'); r.add_argument('command'); r.add_argument('--timeout', type=float, default=20)
    s = sub.add_parser('push'); s.add_argument('local'); s.add_argument('remote')
    b = sub.add_parser('boot'); b.add_argument('seconds', nargs='?', type=float, default=60)
    a = p.parse_args()
    dev = a.dev or (sorted(glob.glob('/dev/ttyUSB*')) or [None])[0]
    if not dev:
        sys.exit('nenhum /dev/ttyUSB*: cabo no Mini-B "UART" e placa ligada?')
    u = Uart(dev, a.log)
    if a.cmd == 'run':
        sys.stdout.write(u.run(a.command, a.timeout))
    elif a.cmd == 'push':
        n, md5 = u.push(a.local, a.remote)
        print('%s -> %s: %d bytes, md5 %s ok' % (a.local, a.remote, n, md5))
    elif a.cmd == 'boot':
        sys.stdout.write(u.read(a.seconds))


if __name__ == '__main__':
    main()
