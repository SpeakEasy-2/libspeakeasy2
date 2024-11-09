.PHONY: all
all: build

build: CMAKE_BUILD_TYPE := Release
build: cmake-build

debug: CMAKE_BUILD_TYPE := Debug
debug: cmake-debug

asan: CMAKE_BUILD_TYPE := Sanitizer
asan: cmake-asan

cmake-%: init-%
	cmake --build $*

init-%:
	cmake -B $* . -G Ninja -D CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

valgrind-%: debug
	valgrind --track-origins=yes --leak-check=full debug/examples/$*

.PHONY: clean
clean:
	rm -f vgcore.*

.PHONY: clean-dist
clean-dist: clean
	rm -rf build
	rm -rf debug
	rm -rf asan
