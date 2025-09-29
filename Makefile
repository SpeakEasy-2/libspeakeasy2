.PHONY: all
all: build

.PHONY: build
build: CMAKE_BUILD_TYPE := Release
build: cmake-build

.PHONY: debug
debug: CMAKE_BUILD_TYPE := Debug
debug: cmake-debug

.PHONY: asan
asan: CMAKE_BUILD_TYPE := Sanitizer
asan: cmake-asan

.PHONY: profile
profile: CMAKE_BUILD_TYPE := Profile
profile: cmake-profile

cmake-%: init-%
	cmake --build $*

init-%:
	cmake -B $* . -G Ninja -D CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

valgrind-%: debug
	valgrind --track-origins=yes --leak-check=full debug/examples/$*

.PHONY: clean
clean:
	rm -f vgcore.*
	rm -f perf.data
	rm -f perf.data.old

.PHONY: clean-dist
clean-dist: clean
	rm -rf build
	rm -rf debug
	rm -rf asan
