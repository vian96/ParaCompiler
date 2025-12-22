.PHONY: all config build clean clean-gen run test test-gen test-all rebuild

all: build

config:
	cmake --preset dev

build:
	cmake --build --preset dev

test:
	cmake --build --preset dev --target check-lit

test-gen:
	cmake --build --preset dev --target check-gen

test-all:
	cmake --build --preset dev --target check-all

clean:
	rm -rf build src/generated
	find . -name "__pycache__" -type d -exec rm -rf {} +

clean-gen:
	rm -rf tests/gen

run: build
	./build/src/paracl $(ARGS) | lli -load=./build/src/libparastdlib.so

rebuild: clean config build
