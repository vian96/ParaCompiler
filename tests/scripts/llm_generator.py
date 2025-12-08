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

BASE_GEN_DIR = "tests/gen"
PCL_DIR = os.path.join(BASE_GEN_DIR, "pcl")
CRASH_DIR = os.path.join(BASE_GEN_DIR, "crashes")

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
                SEEN_HASHES.add(get_code_hash(f.read()))
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
    prefix = random.choice(["v", "val", "res", "tmp", "data"])
    suffix = ''.join(random.choices(string.ascii_lowercase, k=3))
    return f"{prefix}_{suffix}"

def generate_test_pair(index: int) -> Optional[Dict[str, Any]]:
    v1 = get_random_var_name()
    v2 = get_random_var_name()
    val1 = random.randint(-1000, 1000)
    val2 = random.randint(-1000, 1000)

    scenarios = [
        f"Assign {val1} to {v1}, output it.",
        f"Assign {val1} to {v1}, assign {v1} to {v2}, output {v2}.",
        f"Multiple outputs: {v1}={val1}, {v2}={val2}, output both.",
        f"Reassignment: {v1}={val1}, output it, then {v1}={val2}, output it."
    ]
    task = random.choice(scenarios)

    prompt = f"""
    [INST] You are a test generator for the ParaCL compiler.

    HERE IS THE OFFICIAL GRAMMAR (Read it carefully):
    ```antlr
    {RAW_GRAMMAR}
    ```

    CRITICAL RULES (PHASE 0):
    1. **NO C-STYLE DECLARATIONS**.
       WRONG: `int {v1} = {val1};`
       WRONG: `{v1} int = {val1};`
       RIGHT: `{v1} : int = {val1};`

    2. **NO ARITHMETIC** (Compiler crashes on +, -, *, /).
       WRONG: `x = 5 + 5;`
       RIGHT: `x = 10;`

    3. **Syntax**:
       - Output: `output(0, var_name);`
       - Assign: `var_name = 123;`

    Task: Generate a test for: "{task}"

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
            "options": {"temperature": 0.5}
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
    total_attempts = 15

    for i in range(total_attempts):
        logger.info(f"--- Iteration #{i} ---")

        data = generate_test_pair(i)
        if not data:
            continue

        py_code = clean_code(data.get("python_code"))
        pcl_code = clean_code(data.get("paracl_code"))

        if not py_code or not pcl_code:
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

        check_lines = "\n".join([f"// CHECK: {line}" for line in expected_out.splitlines()])
        lit_header = f"// RUN: %paracl %s | FileCheck %s\n"

        with open(pcl_path, "w") as f:
            f.write(lit_header)
            f.write(pcl_code)
            f.write("\n\n")
            f.write(check_lines)
            f.write("\n")

        ret_code, err_msg = run_compiler_check(pcl_path)

        if ret_code != 0:
            if ret_code > 100 or ret_code < 0:
                logger.critical(f"CRASH (code {ret_code})")
                os.rename(pcl_path, os.path.join(CRASH_DIR, f"{base_name}.pcl"))
            else:
                one_line_err = err_msg.strip().replace('\n', ' | ')[:120]
                logger.warning(f"Syntax error: {one_line_err}...")
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
