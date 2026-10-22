#!/bin/sh
# Desativa (ou restaura) o app `spider` da Terasic no /etc/inittab da placa, pela UART.
# O aracno e o spider mapeiam os mesmos registradores PWM: não podem rodar juntos.
# Uso: tools/board-spider-app.sh disable | restore | status
set -eu
cd "$(dirname "$0")/.."
U="python3 tools/uart.py"
case "${1:-status}" in
  disable)
    $U run "[ -f /etc/inittab.orig ] || cp /etc/inittab /etc/inittab.orig; sed -i 's|^21:5:wait:/home/root/./spider|#&|' /etc/inittab; killall spider 2>/dev/null; sync; grep -n spider /etc/inittab"
    ;;
  restore)
    $U run "[ -f /etc/inittab.orig ] && cp /etc/inittab.orig /etc/inittab; sync; grep -n spider /etc/inittab"
    ;;
  status)
    $U run "grep -n spider /etc/inittab; ps | grep -c '[s]pider'"
    ;;
  *) echo "uso: $0 disable|restore|status" >&2; exit 2 ;;
esac
