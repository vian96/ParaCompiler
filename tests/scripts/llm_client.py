import requests
import json
import random
from typing import Optional, Dict, Any

import config
from utils import logger, get_random_var_name

def generate_test_pair(index: int) -> Optional[Dict[str, Any]]:
    limit_val = random.randint(5, 15)
    var_name = get_random_var_name()

    raw_scenario = random.choice(config.TEST_SCENARIOS)

    try:
        task = raw_scenario.format(limit=limit_val, var_name=var_name)
    except KeyError as e:
        logger.warning(f"Scenario template error: missing key {e}")
        task = raw_scenario

    prompt = config.SYSTEM_PROMPT_TEMPLATE.format(
        spec=config.PARACL_SPEC,
        task=task
    )

    try:
        resp = requests.post(config.OLLAMA_URL, json={
            "model": config.MODEL,
            "prompt": prompt,
            "stream": False,
            "format": "json",
            "options": {"temperature": config.TEMPERATURE}
        }).json()
        return json.loads(resp['response'])
    except Exception as e:
        logger.error(f"Generation failed: {e}")
        return None
