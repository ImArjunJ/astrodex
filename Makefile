# AstroSplat — top-level build wrapper around CMake
#
# Usage:
#   make              build Release (default)
#   make debug        build Debug
#   make clean        nuke build/
#   make rebuild      clean + build
#   make run          build + launch astrosplat
#   make explorer     build + launch starexplorer
#   make test         build + run CTest
#   make shaders      recompile SPIR-V shaders only
#   make asan         build Debug with AddressSanitizer

BUILD_DIR   := build
BUILD_TYPE  := Release
JOBS        := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CMAKE_FLAGS :=

.PHONY: all debug release clean rebuild run explorer test shaders asan configure web serve

all: release

release: BUILD_TYPE := Release
release: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

debug: BUILD_TYPE := Debug
debug: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

asan: BUILD_TYPE := Debug
asan: CMAKE_FLAGS += -DASTROCORE_ENABLE_ASAN=ON
asan: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

configure:
	@# Nuke stale cache if it was generated from a different source directory
	@if [ -f $(BUILD_DIR)/CMakeCache.txt ]; then \
		cached_src=$$(grep 'CMAKE_HOME_DIRECTORY' $(BUILD_DIR)/CMakeCache.txt 2>/dev/null | cut -d= -f2); \
		if [ -n "$$cached_src" ] && [ "$$cached_src" != "$$(pwd)" ]; then \
			echo "-- Build cache from $$cached_src, reconfiguring for $$(pwd)"; \
			rm -rf $(BUILD_DIR); \
		fi; \
	fi
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

run: release
	@cd $(BUILD_DIR) && ./astrosplat

explorer: release
	@cd $(BUILD_DIR) && ./starexplorer

test: debug
	cd $(BUILD_DIR) && ctest --output-on-failure -j$(JOBS)

shaders: configure
	cmake --build $(BUILD_DIR) --target astrosplat_shaders -j$(JOBS)

web:
	emcmake cmake -B build_web -DCMAKE_BUILD_TYPE=Release
	cmake --build build_web -j$(JOBS)
	@echo "Open build_web/astrodex.html in a WebGPU-enabled browser"

serve: web
	cd build_web && python3 -m http.server 8080
