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

test: build
	./build/src/paracl ./test.pcl

rebuild: clean config build
