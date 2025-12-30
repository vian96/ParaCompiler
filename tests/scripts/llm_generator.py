import subprocess
import requests
import re
import time
import json
import random
import hashlib
import string
import logging
from pathlib import Path
from rich.logging import RichHandler
from typing import Optional, Dict, Any, Tuple, Set

OLLAMA_URL = "http://localhost:11434/api/generate"
MODEL = "qwen2.5-coder:14b"
COMPILER_BIN = Path("build/src/paracl")
GRAMMAR_FILE = Path("src/grammar/ParaCL.g4")

GENERATE_LIT_HEADERS = True

BASE_GEN_DIR = Path("tests/gen")
PCL_DIR = BASE_GEN_DIR / "pcl"
CRASH_DIR = BASE_GEN_DIR / "crashes"

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
// input(0) is supported but avoid using it for deterministic testing.

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

logging.basicConfig(
    level=logging.INFO,
    format="%(message)s",
    datefmt="[%X]",
    handlers=[RichHandler(rich_tracebacks=True)]
)
logger = logging.getLogger(__name__)

PCL_DIR.mkdir(parents=True, exist_ok=True)
CRASH_DIR.mkdir(parents=True, exist_ok=True)

SEEN_HASHES: Set[str] = set()

try:
    RAW_GRAMMAR = GRAMMAR_FILE.read_text(encoding="utf-8")
except Exception as e:
    logger.error(f"Error reading grammar file: {e}")
    RAW_GRAMMAR = "varDecl: ID ':' typeSpec? '=' expr;"

def get_code_hash(code_text: str) -> str:
    normalized = re.sub(r"\s+", "", code_text)
    return hashlib.md5(normalized.encode()).hexdigest()

def load_existing_tests() -> None:
    logger.info(f"Scanning {PCL_DIR} for history...")
    files = list(PCL_DIR.glob("*.pcl"))
    for filepath in files:
        try:
            content = filepath.read_text(encoding="utf-8")
            clean_content = re.sub(r"//.*", "", content)
            SEEN_HASHES.add(get_code_hash(clean_content))
        except Exception as e:
            logger.warning(f"Could not read {filepath}: {e}")
    logger.info(f"Loaded {len(SEEN_HASHES)} existing tests.")

def clean_code(text: Optional[str]) -> str:
    if not text:
        return ""
    match = re.search(r"```(?:python|paracl|pcl)?\n(.*?)```", text, re.DOTALL)
    if match:
        return match.group(1).strip()
    return text.replace("```", "").strip()

def get_random_var_name() -> str:
    prefix = random.choice(["v", "val", "res", "tmp", "cnt", "iter"])
    suffix = ''.join(random.choices(string.ascii_lowercase, k=2))
    return f"{prefix}_{suffix}"

def generate_test_pair(index: int) -> Optional[Dict[str, Any]]:
    limit = random.randint(5, 15)
    v1 = get_random_var_name()

    scenarios = [
        f"Arithmetic: Integer division and precedence logic with variables ({v1}).",
        f"Loop: Sum of even numbers from 0 to {limit} using 'for (i in 0:{limit})'.",
        f"Logic: While loop calculating a sequence (e.g. n = n - 1) until 0.",
        f"Function: A simple recursive function (like factorial or sum) called with {random.randint(3,6)}.",
        f"Structure: Create a struct using glue(), modify a field, and output it.",
        f"Conditionals: Nested if/else checking values of an integer expression.",
        f"Complex: A function that takes a struct field and returns an int."
    ]
    task = random.choice(scenarios)

    prompt = f"""
    [INST] You are an Expert Compiler Tester for a C-like language called **ParaCL**.

    --- CRITICAL SEMANTICS ---
    1. **NO FLOATS**: The language ONLY has `int`. DO NOT generate float numbers (e.g., 3.5).
    2. **INTEGER DIVISION**: `5 / 2` equals `2` (not 2.5).
    3. **STRICT ORACLE**: You must generate a Python script to verify the output.
       **IMPORTANT**: Since ParaCL uses integer division, your Python code MUST use `//` for division (e.g., `a // b`) to match the logic.

    --- IMPLEMENTATION STATUS ---
    {PARACL_SPEC}

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

    try:
        resp = requests.post(OLLAMA_URL, json={
            "model": MODEL,
            "prompt": prompt,
            "stream": False,
            "format": "json",
            "options": {"temperature": 0.8}
        }).json()
        return json.loads(resp['response'])
    except Exception as e:
        logger.error(f"Generation failed: {e}")
        return None

def run_oracle(python_code: str) -> Tuple[Optional[str], Optional[str]]:
    try:
        proc = subprocess.run(
            ["python3", "-c", python_code],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=2
        )
        if proc.returncode != 0:
            return None, proc.stderr
        return proc.stdout.strip(), None
    except Exception as e:
        return None, str(e)

def run_compiler_check(pcl_path: Path) -> Tuple[int, str]:
    try:
        proc = subprocess.run(
            [COMPILER_BIN, pcl_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return proc.returncode, proc.stderr
    except FileNotFoundError:
        return -1, "Binary not found"

def main() -> None:
    load_existing_tests()
    logger.info(f"Starting LLM test generator, using {MODEL}...")

    success_count = 0
    total_attempts = 20

    for i in range(total_attempts):
        logger.info(f"--- Iteration #{i} ---")

        data = generate_test_pair(i)
        if not data:
            continue

        py_code = clean_code(data.get("python_code"))
        pcl_code = clean_code(data.get("paracl_code"))

        if not py_code or not pcl_code:
            logger.warning("Empty code received")
            continue

        code_hash = get_code_hash(pcl_code)
        if code_hash in SEEN_HASHES:
            logger.info("Duplicate detected (skipping)")
            continue

        expected_out, err = run_oracle(py_code)
        if err:
            logger.error(f"Oracle failed: {err}")
            continue

        base_name = f"test_{int(time.time())}_{i}"
        pcl_path = PCL_DIR / f"{base_name}.pcl"

        content = []
        if GENERATE_LIT_HEADERS:
            content.append("// RUN: %paracl %s > %t.ll")
            content.append("// RUN: %lli -load=%lib %t.ll | FileCheck %s")

        content.append(pcl_code)

        if GENERATE_LIT_HEADERS and expected_out:
            content.append("\n")
            for line in expected_out.splitlines():
                if line.strip():
                    content.append(f"// CHECK: {line}")

        content.append("")

        pcl_path.write_text("\n".join(content), encoding="utf-8")

        ret_code, err_msg = run_compiler_check(pcl_path)

        if ret_code != 0:
            if ret_code > 100 or ret_code < 0:
                logger.critical(f"CRASH (code {ret_code})")
                pcl_path.rename(CRASH_DIR / pcl_path.name)
            else:
                one_line_err = err_msg.strip().replace('\n', ' | ')[:120]
                logger.warning(f"Compiler Error: {one_line_err}...")
                logger.debug("--- FAILED CODE ---\n" + pcl_code + "\n-------------------")
                if pcl_path.exists():
                    pcl_path.unlink()
        else:
            logger.info(f"Saved valid test: {base_name}")
            SEEN_HASHES.add(code_hash)
            success_count += 1

    logger.info(f"Done. Added {success_count} tests.")

if __name__ == "__main__":
    main()
