# ParaCompiler
This is the ParaCL compiler. It's a project we built to compile a custom C-like language into LLVM IR.

We went a bit crazy with modern C++, so under the hood, it uses C++ modules; because of this, the build requirements are pretty strict.

## Implementation status
**Done:**
- control flow
- variable width integers (any width)
- structs (except for explicit type specification, only using glue)
- functions

**In progress:**
- arrays, vectors
- templates
- floats
- repeat, bind, methods

***Note:***
- integer literals types are taken from operands that they're used with. e.g. if you add a big number with int(8), this number will be truncated
- integers can only be extended, not truncated (implicit conversion to bool exists though)
- && and || are actually bit-wise
- shadowing variables from outer scopes is not supported because ParaCL grammatic is too ambigious. On the other hand, you cannot access any external variable from function (therefore, you cannot write recursive functions)
- implicit conversion in return statement is not supported

## Getting the Code
We use the Google `repo` tool to manage dependencies (like the grammar and scripts), because we can (hope you didn't expect a better explanation).

1. Install `repo` if you haven't already.
2. Initialize and sync:

```bash
mkdir paracl_project && cd paracl_project
repo init -u https://github.com/nerett/paracompiler-manifest -b main
repo sync
```

This will fetch the compiler source (`paracl`), the infrastructure scripts (`tools`), and external dependencies.

## Environment Setup
Since C++ Modules support in system compilers is often hit-or-miss (mostly miss), we strongly recommend using our custom toolchain. It prevents headaches with `libc++` vs `libstdc++` conflicts.

You can go samurai way with your own LLVM (18.0+), ANTLR (1.13.1) and clang, but we don't recommend that. You must ensure your clang is using `libc++` and that the LLVM libraries you link against were also built with `libc++`. Mixing standard libraries will cause linker errors.

Navigate to the [tools](https://github.com/nerett/paracompiler-tools) directory and choose your weapon:

### Option 1: I just want to run it (local)
Download our pre-built LLVM 22 & ANTLR binary set. It puts everything in `external/dist` without touching your system paths.

```bash
cd tools
make pull-toolchain
```

### Option 2: I love containers
Run an interactive shell inside our CI container where everything is already set up.

```bash
cd tools
make pull-img-ci
make shell-ci
# you're inside paracl/ now
```

*For more details on how we build this toolchain, check out the [tools](github.com/nerett/paracompiler-tools).*

## Requirements
Besides LLVM & ANTLR-runtime toolchain you need a Linux environment with (or use our container image):

- Stable & fast internet connection
- CMake 4.1.0+ (we use 4.2.0; 3.30+ can also be ok, but you'll need to set magic UUID yourself)
- Ninja 1.11+
- Python 3.10+ (for generating and running tests)
- Python dependencies; run:

```bash
python3 -m pip install -r requirements.txt
```

- Java (runtime is enough, just for ANTLR jar)

## How to Build
We made a Makefile to wrap the CMake commands so you don't have to type long strings.

To configure (this pulls ANTLR jar and sets up Ninja):

```bash
make config
```

To build:
``` bash
make build
```

If you used **Option 1** above, CMake will automatically detect the custom toolchain in `../external/dist`.

## Running Tests
We use LIT (LLVM Integrated Tester).

### Regression tests

```bash
make test
```

### Generated tests
Generate some new tests with:

```bash
python tests/scripts/llm_generator.py
```

Run them:

```bash
make test-gen
```

## Troubleshooting

> CMake says it can't find LLVM

If you are using the custom toolchain (`make pull-toolchain`), ensure you ran `make config` *after* downloading the toolchain.
If you are using system LLVM, make sure `llvm-config` is in your PATH.

> Modules errors / "std module not found"

If you see weird errors about `.pcm` files or missing `std` module:
1. Try doing a clean build: `make rebuild`.
2. Ensure you are not mixing system GCC headers with Clang modules. Using the `tools` or `tools-ci` container usually fixes this instantly.

> Linker errors (undefined reference to std::__cxx11...)

You are likely linking object files built with `libc++` against libraries built with `libstdc++`. Use the provided toolchain or container to guarantee ABI compatibility.
