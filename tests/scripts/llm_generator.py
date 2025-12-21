import os
import subprocess
import requests
import re
import time
import json
import random
import hashlib
import string
import glob
import logging
from rich.logging import RichHandler
from typing import Optional, Dict, Any, Tuple, Set

OLLAMA_URL = "http://localhost:11434/api/generate"
MODEL = "codestral:22b"
COMPILER_BIN = "./build/src/paracl"
GRAMMAR_FILE = "src/grammar/ParaCL.g4"

GENERATE_LIT_HEADERS = True

BASE_GEN_DIR = "tests/gen"
PCL_DIR = os.path.join(BASE_GEN_DIR, "pcl")
CRASH_DIR = os.path.join(BASE_GEN_DIR, "crashes")

PARACL_SPEC = """
## 1.1 Keywords
input, output, if, else, for, in, int

## 1.2 Types
v : int = 5;
v : int(16) = 10;
By default, int is a 32-bit signed integer.
Integers use two’s complement and wrap on overflow.

## 1.7 Conditionals
if (v2 == 0) output(0, 1);
else { output(0, 2); }

## 1.9 Loops
// Range loop (inclusive start, exclusive end logic usually, but follow C semantics)
for (i in 0:5) output(0, i);

// While loop
while (n < 10) {
  n = n + 1;
  output(0, n);
}

## I/O
output(0, val); // Prints value to stdout with newline
"""

logging.basicConfig(
    level=logging.INFO,
    format="%(message)s",
    datefmt="[%X]",
    handlers=[RichHandler(rich_tracebacks=True)]
)
logger = logging.getLogger(__name__)

os.makedirs(PCL_DIR, exist_ok=True)
os.makedirs(CRASH_DIR, exist_ok=True)

SEEN_HASHES: Set[str] = set()

try:
    with open(GRAMMAR_FILE, "r") as f:
        RAW_GRAMMAR = f.read()
except Exception as e:
    logger.error(f"Error reading grammar file: {e}")
    RAW_GRAMMAR = "varDecl: ID ':' typeSpec? '=' expr;"

def get_code_hash(code_text: str) -> str:
    normalized = re.sub(r"\s+", "", code_text)
    return hashlib.md5(normalized.encode()).hexdigest()

def load_existing_tests() -> None:
    logger.info(f"Scanning {PCL_DIR} for history...")
    files = glob.glob(os.path.join(PCL_DIR, "*.pcl"))
    for filepath in files:
        try:
            with open(filepath, "r") as f:
                content = f.read()
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
    limit = random.randint(5, 20)
    v1 = get_random_var_name()

    scenarios = [
        f"Calculate Factorial of {random.randint(4, 8)} using a while loop.",
        f"Calculate sum of numbers from 1 to {limit} using a for loop (range).",
        f"Fibonacci sequence: print first {random.randint(5, 10)} numbers.",
        f"Nested loops: Multiplication table for numbers up to 3.",
        f"Complex arithmetic with precedence: ({v1} * 2 + 5) / 3 - {v1} (initialize {v1} first).",
        f"If-Else logic: Loop from 0 to 10, print 1 if even, 0 if odd.",
        f"Sum of squares from 1 to {random.randint(4, 8)}.",
        f"Collatz conjecture step: if {v1} is even div by 2, else mult by 3 plus 1 (perform a few steps)."
    ]
    task = random.choice(scenarios)

    prompt = f"""
    [INST] You are an Expert Polyglot Programmer and Compiler Tester.

    Target Language: **ParaCL**.

    --- LANGUAGE SPECIFICATION ---
    {PARACL_SPEC}

    --- ANTLR GRAMMAR (Syntax Reference) ---
    ```antlr
    {RAW_GRAMMAR}
    ```

    --- IMPLEMENTATION STATUS (What works NOW) ---
    1. **Types**: Only `int` is fully stable. Avoid `double`/`float` for now.
    2. **Logic**: `if`, `else`, `while`, `for (i in start:end)`.
    3. **Operators**: `+`, `-`, `*`, `/`, `&&`, `||`, `<`, `>`, `==`.
    4. **I/O**: `output(0, val)`.
    5. **Input**: DO NOT USE `input()`. Use hardcoded variable initialization (e.g., `n : int = 10;`).

    --- TASK ---
    Create a logic test for: **"{task}"**

    Requirements:
    1. Write equivalent code in **Python** (Reference) and **ParaCL**.
    2. **Determinism**: The output MUST be identical.
    3. **Syntax**:
       - Python: use `print(val)`.
       - ParaCL: use `output(0, val);`. Variables MUST have types (`v : int = ...`).

    Return ONLY JSON:
    {{
        "python_code": "...",
        "paracl_code": "..."
    }}
    [/INST]
    """

    try:
        resp = requests.post(OLLAMA_URL, json={
            "model": MODEL,
            "prompt": prompt,
            "stream": False,
            "format": "json",
            "options": {"temperature": 1.0}
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

def run_compiler_check(pcl_path: str) -> Tuple[int, str]:
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
        pcl_path = os.path.join(PCL_DIR, f"{base_name}.pcl")

        content = []
        if GENERATE_LIT_HEADERS:
            content.append(f"// RUN: %paracl %s | FileCheck %s")

        content.append(pcl_code)

        if GENERATE_LIT_HEADERS and expected_out:
            content.append("\n")
            for line in expected_out.splitlines():
                if line.strip():
                    content.append(f"// CHECK: {line}")

        content.append("")

        with open(pcl_path, "w") as f:
            f.write("\n".join(content))

        ret_code, err_msg = run_compiler_check(pcl_path)

        if ret_code != 0:
            if ret_code > 100 or ret_code < 0:
                logger.critical(f"CRASH (code {ret_code})")
                os.rename(pcl_path, os.path.join(CRASH_DIR, f"{base_name}.pcl"))
            else:
                one_line_err = err_msg.strip().replace('\n', ' | ')[:120]
                logger.warning(f"Compiler Error: {one_line_err}...")
                logger.debug("--- FAILED CODE ---\n" + pcl_code + "\n-------------------")
                if os.path.exists(pcl_path):
                    os.remove(pcl_path)
        else:
            logger.info(f"Saved valid test: {base_name}")
            SEEN_HASHES.add(code_hash)
            success_count += 1

    logger.info(f"Done. Added {success_count} tests.")

if __name__ == "__main__":
    main()
