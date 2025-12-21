.PHONY: all config build test clean run test test_run

all: build

config:
	cmake --preset dev

build:
	cmake --build --preset dev

test:
	ctest --preset dev

clean:
	rm -rf build src/generated

run: build
	./build/src/paracl $(ARGS) | lli -load=./build/src/libparastdlib.so

test_run:
	./build/src/paracl ./test.pcl --dump-ast

test: test_run

rebuild: clean config build
