import subprocess
import time
import argparse
import random
from typing import Tuple, Optional, Set

import config
from utils import logger, get_code_hash, clean_code
from llm_client import generate_fresh_test, mutate_existing_test

SEEN_HASHES: Set[str] = set()

def load_history() -> None:
    logger.info(f"Scanning {config.PCL_DIR} for history...")

    config.PCL_DIR.mkdir(parents=True, exist_ok=True)
    config.CRASH_DIR.mkdir(parents=True, exist_ok=True)

    files = list(config.PCL_DIR.glob("*.pcl"))
    for filepath in files:
        try:
            content = filepath.read_text(encoding="utf-8")
            clean_content = clean_code(content)
            SEEN_HASHES.add(get_code_hash(clean_content))
        except Exception as e:
            logger.warning(f"Could not read {filepath}: {e}")
    logger.info(f"Loaded {len(SEEN_HASHES)} existing tests.")

def sanitize_python_code(py_code: str) -> str:
    """Fix common LLM hallucinations in Python code."""
    lines = []
    for line in py_code.splitlines():
        stripped = line.strip()
        if stripped.startswith("//"):
            line = line.replace("//", "#", 1)

        if "output(" in line and "print(" not in line:
            line = line.replace("output(0,", "print(")
            line = line.replace("output(", "print(")

        lines.append(line)
    return "\n".join(lines)

def run_oracle(python_code: str) -> Tuple[Optional[str], Optional[str]]:
    cleaned_py_code = sanitize_python_code(python_code)

    try:
        proc = subprocess.run(
            ["python3", "-c", cleaned_py_code],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=config.ORACLE_TIMEOUT
        )
        if proc.returncode != 0:
            return None, proc.stderr
        return proc.stdout.strip(), None
    except Exception as e:
        return None, str(e)

def run_compiler_check(pcl_path: config.Path) -> Tuple[int, str]:
    try:
        proc = subprocess.run(
            [config.COMPILER_BIN, pcl_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return proc.returncode, proc.stderr
    except FileNotFoundError:
        return -1, "Binary not found"

def get_mutation_sources() -> list[str]:
    sources = []
    if config.LIT_DIR.exists():
        for p in config.LIT_DIR.glob("*.pcl"):
            try:
                content = p.read_text(encoding="utf-8")
                # Filter out RUN/CHECK lines to avoid confusing LLM
                lines = [line for line in content.splitlines()
                         if not line.strip().startswith("// RUN")
                         and not line.strip().startswith("// CHECK")]
                sources.append("\n".join(lines))
            except Exception as e:
                logger.warning(f"Failed to read mutation source {p}: {e}")
    return sources

def run_fuzzing_mode():
    sources = get_mutation_sources()
    if not sources:
        logger.error("No input tests found in tests/lit to fuzz.")
        return

    logger.info(f"Loaded {len(sources)} valid tests for mutation base.")
    success_count = 0

    for i in range(config.TOTAL_ATTEMPTS):
        logger.info(f"--- Fuzz Iteration #{i} ---")

        base_code = random.choice(sources)
        data = mutate_existing_test(base_code)
        if not data:
            continue

        py_code = clean_code(data.get("python_code"))
        pcl_code = clean_code(data.get("paracl_code"))

        if not py_code or not pcl_code:
            logger.warning("Empty code received")
            continue

        pcl_code = "\n".join([line for line in pcl_code.splitlines() if "// CHECK" not in line])

        code_hash = get_code_hash(pcl_code)
        if code_hash in SEEN_HASHES:
            logger.info("Duplicate detected (skipping)")
            continue

        expected_out, err = run_oracle(py_code)
        if err:
            logger.error(f"Oracle failed: {err}")
            continue

        if not expected_out:
            logger.warning("Oracle produced empty output. Skipping.")
            continue

        base_name = f"fuzz_{int(time.time())}_{i}"
        pcl_path = config.PCL_DIR / f"{base_name}.pcl"

        content = []
        if config.GENERATE_LIT_HEADERS:
            content.append("// RUN: %paracl %s > %t.ll")
            content.append("// RUN: %lli -load=%lib %t.ll | FileCheck %s")

        content.append(pcl_code)

        if config.GENERATE_LIT_HEADERS:
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
                pcl_path.rename(config.CRASH_DIR / pcl_path.name)
            else:
                one_line_err = err_msg.strip().replace('\n', ' | ')[:120]
                logger.warning(f"Compiler Error: {one_line_err}...")
                logger.debug("--- FAILED CODE ---\n" + pcl_code + "\n-------------------")
                if pcl_path.exists():
                    pcl_path.unlink()
        else:
            logger.info(f"Saved valid FUZZ test: {base_name}")
            SEEN_HASHES.add(code_hash)
            success_count += 1

    logger.info(f"Done. Added {success_count} fuzz tests.")

def run_generation_mode():
    success_count = 0

    for i in range(config.TOTAL_ATTEMPTS):
        logger.info(f"--- Iteration #{i} ---")

        data = generate_fresh_test(i)
        if not data:
            continue

        py_code = clean_code(data.get("python_code"))
        pcl_code = clean_code(data.get("paracl_code"))

        if not py_code or not pcl_code:
            logger.warning("Empty code received")
            continue

        pcl_code = "\n".join([line for line in pcl_code.splitlines() if "// CHECK" not in line])

        code_hash = get_code_hash(pcl_code)
        if code_hash in SEEN_HASHES:
            logger.info("Duplicate detected (skipping)")
            continue

        expected_out, err = run_oracle(py_code)
        if err:
            logger.error(f"Oracle failed: {err}")
            continue

        if not expected_out:
            logger.warning("Oracle produced empty output. Skipping.")
            continue

        base_name = f"test_{int(time.time())}_{i}"
        pcl_path = config.PCL_DIR / f"{base_name}.pcl"

        content = []
        if config.GENERATE_LIT_HEADERS:
            content.append("// RUN: %paracl %s > %t.ll")
            content.append("// RUN: %lli -load=%lib %t.ll | FileCheck %s")

        content.append(pcl_code)

        if config.GENERATE_LIT_HEADERS:
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
                pcl_path.rename(config.CRASH_DIR / pcl_path.name)
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

def main() -> None:
    parser = argparse.ArgumentParser(description="ParaCL Test Generator")
    parser.add_argument("--fuzz", action="store_true", help="Enable fuzzing mode based on existing tests")
    args = parser.parse_args()

    load_history()
    logger.info(f"Starting LLM Generator using {config.MODEL}...")

    if args.fuzz:
        run_fuzzing_mode()
    else:
        run_generation_mode()

if __name__ == "__main__":
    main()
