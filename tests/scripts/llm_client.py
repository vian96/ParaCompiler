import requests
import json
import random
from typing import Optional, Dict, Any

import config
from utils import logger, get_random_var_name

def query_ollama(prompt: str, temperature: float) -> Optional[Dict[str, Any]]:
    try:
        resp = requests.post(config.OLLAMA_URL, json={
            "model": config.MODEL,
            "prompt": prompt,
            "stream": False,
            "format": "json",
            "options": {"temperature": temperature}
        }).json()
        return json.loads(resp['response'])
    except Exception as e:
        logger.error(f"LLM request failed: {e}")
        return None

def generate_fresh_test(index: int) -> Optional[Dict[str, Any]]:
    limit_val = random.randint(5, 15)
    var_name = get_random_var_name()

    raw_scenario = random.choice(config.TEST_SCENARIOS)

    try:
        task = raw_scenario.format(limit=limit_val, var_name=var_name)
    except KeyError as e:
        logger.warning(f"Scenario template error: missing key {e}")
        task = raw_scenario

    prompt = config.PROMPT_TEMPLATE_GENERATION.format(
        spec=config.PARACL_SPEC,
        task=task
    )

    return query_ollama(prompt, config.TEMPERATURE)

def mutate_existing_test(code_snippet: str) -> Optional[Dict[str, Any]]:
    prompt = config.PROMPT_TEMPLATE_MUTATION.format(input_code=code_snippet)
    return query_ollama(prompt, config.MUTATION_TEMPERATURE)
