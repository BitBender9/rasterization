CXX ?= c++
CC ?= cc
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CFLAGS := -std=c11 -O2 -Wall -Wextra
INCLUDES := -Isrc -Ithird_party/uselibpng
LIBPNG_PREFIX := $(shell if [ -f /opt/homebrew/include/png.h ]; then echo /opt/homebrew; elif command -v brew >/dev/null 2>&1; then brew --prefix libpng 2>/dev/null; fi)
LIBPNG_INCLUDE := $(if $(LIBPNG_PREFIX),-I$(LIBPNG_PREFIX)/include)
LIBPNG_LIBRARY := $(if $(LIBPNG_PREFIX),-L$(LIBPNG_PREFIX)/lib)
SDK_CXX_INCLUDE := $(shell sdk=$$(xcrun --show-sdk-path 2>/dev/null); if [ -f "$$sdk/usr/include/c++/v1/array" ]; then echo "-isystem $$sdk/usr/include/c++/v1"; fi)

.PHONY: build run test clean

build: rasterizer

uselibpng.o: third_party/uselibpng/uselibpng.c third_party/uselibpng/uselibpng.h
	$(CC) $(CFLAGS) $(LIBPNG_INCLUDE) -Ithird_party/uselibpng -c third_party/uselibpng/uselibpng.c -o uselibpng.o

rasterizer: src/main.cpp src/rasterizer.cpp src/rasterizer.hpp uselibpng.o
	$(CXX) $(CXXFLAGS) $(SDK_CXX_INCLUDE) $(INCLUDES) src/main.cpp src/rasterizer.cpp uselibpng.o $(LIBPNG_LIBRARY) -lpng -o rasterizer

run: rasterizer
	./rasterizer $(file)

test: rasterizer
	mkdir -p tests/outputs
	./rasterizer tests/inputs/rast-gray.txt
	./rasterizer tests/inputs/rast-smallgap.txt
	./rasterizer tests/inputs/rast-smoothcolor.txt
	./rasterizer tests/inputs/rast-checkers.txt

clean:
	rm -f rasterizer uselibpng.o tests/outputs/*.png
