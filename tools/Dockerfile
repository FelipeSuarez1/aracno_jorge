# Toolchain cross estática (musl) para o HPS da DE0-Nano-SoC (Cortex-A9, armv7-a hard-float).
# Uso: tools/cross-build.sh  (ou: docker build -t aracno-tc tools/ && docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/w:z -w /w/firmware aracno-tc make)
FROM debian:bookworm-slim

ARG TC_URL=https://musl.cc/arm-linux-musleabihf-cross.tgz
# GCC 11.2.1 20211120, musl.cc; verificado em 2026-10-22
ARG TC_SHA256=11e155a090789c854afb5d91e57c81f441ccdb92f7f277789c3a6d5884a68e58

RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl file make git \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL -o /tmp/tc.tgz "$TC_URL" \
    && echo "$TC_SHA256  /tmp/tc.tgz" | sha256sum -c - \
    && tar -xzf /tmp/tc.tgz -C /opt \
    && rm /tmp/tc.tgz

ENV PATH="/opt/arm-linux-musleabihf-cross/bin:${PATH}"
ENV CROSS=arm-linux-musleabihf-

# git do repo montado em /w pertence a outro uid
RUN git config --global --add safe.directory '*'

WORKDIR /w
CMD ["make", "-C", "firmware"]
