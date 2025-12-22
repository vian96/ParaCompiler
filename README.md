# ParaCompiler
This is the ParaCL compiler. It's a project we built to compile a custom C-like language into LLVM IR.

We went a bit crazy with modern C++, so under the hood, it uses C++ modules; because of this, the build requirements are pretty strict.

## Implementation status
**Done:**
- control flow
- variable width integers (any width)
- structs (except explicit type specification, through glue)
- functions

**In progress:**
- arrays
- vectors
- templates
- floats
- repeat
- methods

## Getting the Code
We use the Google `repo` tool to manage dependencies (like the grammar and scripts), because we can (hope you didn't expect a better explanation).

1. Install `repo` if you haven't already.
2. Initialize and sync:

```bash
mkdir paracl && cd paracl
repo init -u https://github.com/nerett/paracompiler-manifest -b main
repo sync
```

## Requirements
You need a Linux environment. We've tested this on OpenSUSE Leap 15.6 and Ubuntu 22.04.

Here is what you absolutely need:

- Clang-18+ (20+ recommended; we need proper modules support)

- LLVM-20/21 (libraries and llvm-config; package is usually named `llvm20-devel`;)

- CMake 3.28+ (older versions don't play nice with C++ modules)

- Ninja 1.11+ (1.10 and lower don't support modules)

- Python 3.10+ (for running and generating tests)

- Java (runtime is enough, just for ANTLR jar)

- Ollama (if you want to generate even more tests; `codestral:22b` is used by default)

### Python Dependencies
For the testing suite:

```bash
pip install -r requirements.txt
```

## How to Build
We made a Makefile to wrap the CMake commands so you don't have to type long strings.

To configure (this pulls ANTLR and sets up Ninja):

```bash
make config
```

To build:
``` bash
make build
```

If you have any problems with dependencies (e.g. default profile expects `clang++` to be at least `clang++-20`), you can create your own cmake preset in `CMakePresets.json` (or run cmake manually).

## Running Tests
We use LIT (LLVM Integrated Tester).

### Regression tests

```bash
make test
```

### Generated tests
Generate some new tests with

```bash
python tests/scripts/llm_generator.py
```

Run

```bash
make test-gen
```

### Troubleshooting

> CMake says it can't find LLVM 20

Make sure llvm-config-20 is in your PATH, or that LLVM_DIR is set correctly. If you are on Ubuntu, the standard repos usually stop at LLVM 15/16. You'll likely need the nightly packages from apt.llvm.org.

> Modules errors

If you see weird errors about .pcm files, try doing a clean build:

```bash
make rebuild
```

> I don't want to install LLVM 20 on my main PC

Totally fair. Check out the Dockerfile (it's on the way) in the root (or in the tools repo). You can build a container that has everything pre-installed.
