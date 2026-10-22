#!/bin/sh
# Compila o firmware cross estático dentro do container aracno-tc.
# Uso: tools/cross-build.sh [alvos do make]   (ex.: tools/cross-build.sh clean all)
set -eu
cd "$(dirname "$0")/.."
docker image inspect aracno-tc >/dev/null 2>&1 || docker build -t aracno-tc tools/
# :z = rótulo SELinux (Fedora); --user = arquivos gerados ficam do usuário, não de root
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/w:z -w /w/firmware aracno-tc make "$@"
file firmware/build/aracno
