import re
import hashlib
import random
import string
import logging
from typing import Optional
from rich.logging import RichHandler

def setup_logging() -> logging.Logger:
    logging.basicConfig(
        level=logging.INFO,
        format="%(message)s",
        datefmt="[%X]",
        handlers=[RichHandler(rich_tracebacks=True)]
    )
    return logging.getLogger("llm_gen")

logger = setup_logging()

def get_code_hash(code_text: str) -> str:
    """Calculates MD5 hash of code ignoring whitespace to detect duplicates."""
    normalized = re.sub(r"\s+", "", code_text)
    return hashlib.md5(normalized.encode()).hexdigest()

def clean_code(text: Optional[str]) -> str:
    """Extracts code from markdown blocks if present."""
    if not text:
        return ""
    match = re.search(r"```(?:python|paracl|pcl)?\n(.*?)```", text, re.DOTALL)
    if match:
        return match.group(1).strip()
    return text.replace("```", "").strip()

def get_random_var_name() -> str:
    """Generates a random variable name to avoid collisions in generated code."""
    prefix = random.choice(["v", "val", "res", "tmp", "cnt", "iter"])
    suffix = ''.join(random.choices(string.ascii_lowercase, k=2))
    return f"{prefix}_{suffix}"
