# SPDX-FileCopyrightText: 2025 Ben Jarvis
#
# SPDX-License-Identifier: LGPL-2.1-only

CMAKE_ARGS := -G Ninja -DCMAKE_C_COMPILER=gcc
CMAKE_ARGS_RELEASE := -DCMAKE_BUILD_TYPE=Release
CMAKE_ARGS_DEBUG := -DCMAKE_BUILD_TYPE=Debug
CTEST_ARGS := --output-on-failure

default: release

.PHONY: build_release
build_release: 
	@mkdir -p build/release
	@cmake ${CMAKE_ARGS} ${CMAKE_ARGS_RELEASE} -S . -B build/release
	@ninja -C build/release

.PHONY: build_debug
build_debug:
	@mkdir -p build/debug
	@cmake ${CMAKE_ARGS} ${CMAKE_ARGS_DEBUG} -S . -B build/debug
	@ninja -C build/debug

.PHONY: test_debug
test_debug: build_debug
	cd build/debug && ctest ${CTEST_ARGS}

.PHONY: test_release
test_release: build_release
	cd build/release && ctest ${CTEST_ARGS}

.PHONY: debug
debug: build_debug test_debug

.PHONY: release
release: build_release test_release


.PHONY: syntax-check
syntax-check:
	@find src/ -type f \( -name "*.c" -o -name "*.h" \) -print0 | \
                xargs -0 -I {} sh -c 'uncrustify -c etc/uncrustify.cfg --check {} >/dev/null 2>&1 || (echo "Formatting issue in: {}" && exit 1)' || exit 1


.PHONY: syntax
syntax:
	@find src/ -type f \( -name "*.c" -o -name "*.h" \) -print0 | \
                xargs -0 -I {} sh -c 'uncrustify -c etc/uncrustify.cfg --replace --no-backup {}' >/dev/null 2>&1
		
clean:
	@rm -rf build
		
