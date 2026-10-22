// aracno — binário do HPS (DE0-Nano-SoC). S02: hello world estático.
// Prova que a toolchain musl gera algo que roda na imagem Yocto 2015.
#include <cstdio>
#include <ctime>

#ifndef ARACNO_GIT_REV
#define ARACNO_GIT_REV "unknown"
#endif

int main() {
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        std::perror("clock_gettime");
        return 1;
    }
    std::printf("aracno ok rev=%s monotonic=%ld.%09ld\n",
                ARACNO_GIT_REV, static_cast<long>(ts.tv_sec), ts.tv_nsec);
    return 0;
}
