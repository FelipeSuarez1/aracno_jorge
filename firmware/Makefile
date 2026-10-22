# Build cross (estático, musl) do binário do HPS e testes de host.
# Cross:  tools/cross-build.sh
# Host:   make test

CROSS   ?= arm-linux-musleabihf-
CXX      = $(CROSS)g++
HOSTCXX ?= g++
GIT_REV := $(shell git -C .. rev-parse --short HEAD 2>/dev/null || echo unknown)

CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
           -march=armv7-a -mfpu=neon -mfloat-abi=hard \
           -Iinclude -DARACNO_GIT_REV=\"$(GIT_REV)\"
LDFLAGS  = -static -no-pie -s

SRC   := $(wildcard src/*.cpp)
OBJ   := $(patsubst src/%.cpp,build/%.o,$(SRC))
BIN   := build/aracno

TEST_SRC := $(wildcard ../tests/*.cpp)
TEST_BIN := $(patsubst ../tests/%.cpp,../tests/build/%,$(TEST_SRC))
HOSTFLAGS = -std=c++11 -O1 -g -Wall -Wextra -Werror -Iinclude -DARACNO_GIT_REV=\"$(GIT_REV)\"

.PHONY: all test clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@file $@ 2>/dev/null || true

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

# Testes de host: um executável por tests/*.cpp; cada um deve retornar 0.
test: $(TEST_BIN)
	@if [ -z "$(TEST_BIN)" ]; then echo "0 testes em tests/ (aceitável até S03)"; fi
	@set -e; for t in $(TEST_BIN); do echo "== $$t"; $$t; done; echo "testes ok"

../tests/build/%: ../tests/%.cpp $(filter-out src/main.cpp,$(SRC)) | ../tests/build
	$(HOSTCXX) $(HOSTFLAGS) $^ -o $@

../tests/build:
	mkdir -p ../tests/build

clean:
	rm -rf build ../tests/build
