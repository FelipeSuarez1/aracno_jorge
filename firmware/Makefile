# Build cross (estático, musl) do binário do HPS e testes de host.
# Cross:  tools/cross-build.sh
# Host:   make test

CROSS   ?= arm-linux-musleabihf-
CXX      = $(CROSS)g++
HOSTCXX ?= g++
GIT_REV := $(shell git -C .. rev-parse --short HEAD 2>/dev/null || echo unknown)

COMMON   = -std=c++11 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -MMD -MP \
           -Iinclude -Isrc -DARACNO_GIT_REV=\"$(GIT_REV)\"
CXXFLAGS = $(COMMON) -O2 -march=armv7-a -mfpu=neon -mfloat-abi=hard
LDFLAGS  = -static -no-pie -s -lpthread

# Fontes: src/**/*.cpp (um nível de subdiretório). main.cpp fica fora dos testes.
SRC     := $(wildcard src/*.cpp src/*/*.cpp)
LIBSRC  := $(filter-out src/app/main.cpp src/main.cpp,$(SRC))
OBJ     := $(patsubst src/%.cpp,build/%.o,$(SRC))
BIN     := build/aracno

TEST_SRC := $(wildcard ../tests/*.cpp)
TEST_BIN := $(patsubst ../tests/%.cpp,../tests/build/%,$(TEST_SRC))
HOSTFLAGS = $(COMMON) -O1 -g -I../tests -DARACNO_HOST_TEST
HOSTOBJ  := $(patsubst src/%.cpp,../tests/build/obj/%.o,$(LIBSRC))

.PHONY: all test clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@file $@ 2>/dev/null || true

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Testes de host: um executável por tests/*.cpp, linkado com todo src/ menos main.
test: $(TEST_BIN)
	@if [ -z "$(TEST_BIN)" ]; then echo "0 testes em tests/ (aceitável até S03)"; fi
	@set -e; for t in $(TEST_BIN); do echo "== $$t"; $$t; done; echo "testes ok"

../tests/build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(HOSTCXX) $(HOSTFLAGS) -c $< -o $@

../tests/build/%: ../tests/%.cpp $(HOSTOBJ)
	@mkdir -p $(dir $@)
	$(HOSTCXX) $(HOSTFLAGS) $^ -o $@ -lpthread

clean:
	rm -rf build ../tests/build

# Dependências de headers geradas por -MMD (regenerar weights.h recompila policy).
-include $(OBJ:.o=.d) $(HOSTOBJ:.o=.d) $(TEST_BIN:=.d)
