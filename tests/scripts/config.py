from pathlib import Path

OLLAMA_URL = "http://localhost:11434/api/generate"
MODEL = "qwen2.5-coder:14b"
TEMPERATURE = 0.8

PROJECT_ROOT = Path(__file__).parent.parent.parent
COMPILER_BIN = PROJECT_ROOT / "build/src/paracl"
GRAMMAR_FILE = PROJECT_ROOT / "src/grammar/ParaCL.g4"

BASE_GEN_DIR = PROJECT_ROOT / "tests/gen"
PCL_DIR = BASE_GEN_DIR / "pcl"
CRASH_DIR = BASE_GEN_DIR / "crashes"

LIT_DIR = PROJECT_ROOT / "tests/lit"

GENERATE_LIT_HEADERS = True
TOTAL_ATTEMPTS = 20
ORACLE_TIMEOUT = 2.0

PARACL_SPEC = """
## 1. Core Semantics (C++ Style)
- **INTEGER ONLY**: The language currently ONLY supports integers. NO floats, NO doubles.
- **Arithmetic**: `/` is Integer Division (Truncation towards zero). Example: `5 / 2` is `2`.
- **Booleans**: Comparison operators return `1` (true) or `0` (false).

## 2. Supported Syntax
// Variable Declaration
v : int = 10;
x = (v * 2 + 5) / 3; // Type deduction

// I/O
output(0, x); // Prints integer to stdout

// Control Flow
if (x > 5) { ... } else { ... }

while (n > 0) { ... }

// Loops (Range ONLY)
for (i in 0:10) { ... } // i goes from 0 to 9.
// NOTE: 'for (x in array)' is NOT supported yet.

// Functions
func : (x: int) : int = { return x * x; };
res = func(5);

// Structures
s = glue(a: 10, b: 20);
val = s.a;
"""

TEST_SCENARIOS = [
    "Arithmetic: Integer division and precedence logic with variables ({var_name}).",
    "Loop: Sum of even numbers from 0 to {limit} using 'for (i in 0:{limit})'.",
    "Logic: While loop calculating a sequence (e.g. n = n - 1) until 0.",
    "Function: A simple recursive function (like factorial or sum) called with {limit}.",
    "Structure: Create a struct using glue(), modify a field, and output it.",
    "Conditionals: Nested if/else checking values of an integer expression.",
    "Complex: A function that takes a struct field and returns an int."
]

PROMPT_TEMPLATE_GENERATION = """
[INST] You are an Expert Compiler Tester for a C-like language called **ParaCL**.

--- CRITICAL SEMANTICS ---
1. **NO FLOATS**: The language ONLY has `int`. DO NOT generate float numbers (e.g., 3.5).
2. **INTEGER DIVISION**: `5 / 2` equals `2` (not 2.5).
3. **STRICT ORACLE**: You must generate a Python script to verify the output.
   **IMPORTANT**: Since ParaCL uses integer division, your Python code MUST use `//` for division (e.g., `a // b`) to match the logic.

--- IMPLEMENTATION STATUS ---
{spec}

--- TASK ---
Create a test case for: **"{task}"**

Output a JSON object with two fields:
1. `python_code`: A valid Python script that calculates the expected result. **Use `//` for division.** Print the final integer result.
2. `paracl_code`: The equivalent ParaCL code. Use standard `/` operator. Use `output(0, val)`.

Example JSON structure:
{{
    "python_code": "a = 10\\nres = (a * 2 + 5) // 3\\nprint(res)",
    "paracl_code": "a : int = 10;\\nres = (a * 2 + 5) / 3;\\noutput(0, res);"
}}
[/INST]
"""
