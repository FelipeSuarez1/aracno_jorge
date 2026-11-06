#!/bin/sh
# Compila a librealsense v1.12.1 (único SDK com suporte à R200) no Raspberry Pi 4 / Ubuntu 24.04 (GCC 13, kernel 6.8).
# Rodar no Pi: sh build-librealsense.sh   (≈ 5 min no Pi 4). Resultado: ~/librealsense/build/librealsense.so.1.12.1
# Correções em relação ao código de 2017:
#  - includes implícitos que o GCC 13 não traz mais (functional, cstring, sys/sysmacros.h para major/minor...);
#  - librealsense-v1.12.1-aracno.patch: ignora os nós V4L2 de metadados (kernel >= 4.16) olhando device_caps.
# Sem patch de kernel: o uvcvideo do 6.8 já conhece Z16/Y8I/Y12I.
set -eu
D=$(cd "$(dirname "$0")" && pwd)
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq git cmake build-essential libusb-1.0-0-dev pkg-config
[ -d ~/librealsense ] || git clone -q --depth 1 --branch v1.12.1 https://github.com/IntelRealSense/librealsense.git ~/librealsense
cd ~/librealsense
git apply --check "$D/librealsense-v1.12.1-aracno.patch" 2>/dev/null && git apply "$D/librealsense-v1.12.1-aracno.patch"
mkdir -p build && cd build
cmake .. -DBUILD_EXAMPLES=OFF -DBUILD_UNIT_TESTS=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-include functional -include cstring -include cmath -include thread -include atomic -include stdexcept -include limits -include sys/sysmacros.h -fpermissive -w"
make -j4
echo "teste: g++ -O2 -std=c++11 $D/r200_teste.cpp -I ~/librealsense/include -L ~/librealsense/build -lrealsense -Wl,-rpath,\$HOME/librealsense/build -o r200_teste && sudo ./r200_teste"
