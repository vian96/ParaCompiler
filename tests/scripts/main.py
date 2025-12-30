import subprocess
import time
import re
from typing import Tuple, Optional, Set

import config
from utils import logger, get_code_hash, clean_code
from llm_client import generate_test_pair

SEEN_HASHES: Set[str] = set()

def load_existing_tests() -> None:
    logger.info(f"Scanning {config.PCL_DIR} for history...")

    config.PCL_DIR.mkdir(parents=True, exist_ok=True)
    config.CRASH_DIR.mkdir(parents=True, exist_ok=True)

    files = list(config.PCL_DIR.glob("*.pcl"))
    for filepath in files:
        try:
            content = filepath.read_text(encoding="utf-8")
            clean_content = re.sub(r"//.*", "", content)

            SEEN_HASHES.add(get_code_hash(clean_content))
        except Exception as e:
            logger.warning(f"Could not read {filepath}: {e}")
    logger.info(f"Loaded {len(SEEN_HASHES)} existing tests.")

def run_oracle(python_code: str) -> Tuple[Optional[str], Optional[str]]:
    try:
        proc = subprocess.run(
            ["python3", "-c", python_code],
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

def main() -> None:
    load_existing_tests()
    logger.info(f"Starting LLM test generator, using {config.MODEL}...")

    success_count = 0

    for i in range(config.TOTAL_ATTEMPTS):
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
        pcl_path = config.PCL_DIR / f"{base_name}.pcl"

        content = []
        if config.GENERATE_LIT_HEADERS:
            content.append("// RUN: %paracl %s > %t.ll")
            content.append("// RUN: %lli -load=%lib %t.ll | FileCheck %s")

        content.append(pcl_code)

        if config.GENERATE_LIT_HEADERS and expected_out:
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

if __name__ == "__main__":
    main()
