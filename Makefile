.PHONY: all config build test clean run

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
	./build/src/paracl $(ARGS)

test_run: build
	./build/src/paracl ./test.pcl

test: test_run

rebuild: clean config build
