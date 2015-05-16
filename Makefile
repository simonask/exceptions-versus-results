SOURCES = main.cpp parser_with_exceptions.cpp parser_with_results.cpp
DEPS = ${SOURCES} parser.hpp Makefile
.DEFAULT_GOAL := all

GCC5 = g++-5
GCC49 = g++-4.9
CLANG = clang++
CXXFLAGS = -g -std=c++11 -Wall -Wpedantic -Werror

exceptions-versus-results-gcc5-O3: ${DEPS}
	${GCC5} -DCOMPILER=gcc5-O3 ${CXXFLAGS} -O3 -o $@ ${SOURCES}

exceptions-versus-results-gcc5-Os: ${DEPS}
	${GCC5} -DCOMPILER=gcc5-Os ${CXXFLAGS} -Os -o $@ ${SOURCES}

exceptions-versus-results-gcc49-O3: ${DEPS}
	${GCC49} -DCOMPILER=gcc49-O3 ${CXXFLAGS} -O3 -o $@ ${SOURCES}

exceptions-versus-results-gcc49-Os: ${DEPS}
	${GCC49} -DCOMPILER=gcc49-Os ${CXXFLAGS} -Os -o $@ ${SOURCES}

exceptions-versus-results-clang-O3: ${DEPS}
	${CLANG} -DCOMPILER=clang-O3 ${CXXFLAGS} -O3 -o $@ ${SOURCES}

exceptions-versus-results-clang-Os: ${DEPS}
	${CLANG} -DCOMPILER=clang-Os ${CXXFLAGS} -Os -o $@ ${SOURCES}

exceptions-versus-results-rustc: Makefile Cargo.toml src/main.rs src/parser.rs src/benchmark.rs
	cargo build --release
	cp target/release/exceptions-versus-results-rustc .

all: exceptions-versus-results-gcc5-O3 \
	exceptions-versus-results-gcc5-Os \
	exceptions-versus-results-gcc49-O3 \
	exceptions-versus-results-gcc49-Os \
	exceptions-versus-results-clang-O3 \
	exceptions-versus-results-clang-Os \
	exceptions-versus-results-rustc
	@echo
	@echo "compiler;benchmark;µs" > results.csv
	@./exceptions-versus-results-clang-O3 ${ITERATIONS}
	@./exceptions-versus-results-clang-Os ${ITERATIONS}
	@./exceptions-versus-results-gcc5-O3 ${ITERATIONS}
	@./exceptions-versus-results-gcc5-Os ${ITERATIONS}
	@./exceptions-versus-results-gcc49-O3 ${ITERATIONS}
	@./exceptions-versus-results-gcc49-Os ${ITERATIONS}
	@./exceptions-versus-results-rustc ${ITERATIONS}

clean:
	rm -f exceptions-versus-results-gcc5-O3 exceptions-versus-results-gcc5-Os exceptions-versus-results-gcc49-O3 exceptions-versus-results-gcc49-Os exceptions-versus-results-clang-O3 exceptions-versus-results-clang-Os
	rm -rf *.dSYM
	cargo clean

.PHONY := all clean
