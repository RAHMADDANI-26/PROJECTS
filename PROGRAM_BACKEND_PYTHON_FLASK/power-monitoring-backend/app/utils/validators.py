"""
Validation utilities for Power Monitoring Backend.

Provides functions for validating MQTT payloads and JSON data.
"""

import json
import re
from typing import Optional

from app.utils.logger import get_logger

logger = get_logger(__name__)


def validate_json_payload(payload: bytes) -> Optional[dict]:
    """
    Validate and parse a JSON payload from MQTT message.

    Handles chunks, corrupted data, and incomplete payloads.

    Args:
        payload: Raw bytes payload from MQTT message.

    Returns:
        Parsed dictionary if valid, None if invalid.
    """
    if not payload:
        logger.warning("Received empty payload")
        return None

    # Convert bytes to string
    try:
        decoded = payload.decode("utf-8", errors="replace")
    except Exception as e:
        logger.error(f"Failed to decode payload: {e}")
        return None

    # Remove control characters
    decoded = re.sub(r'[\x00-\x08\x0b\x0c\x0e-\x1f]', '', decoded)

    # Try direct parse
    try:
        data = json.loads(decoded)
        if isinstance(data, dict):
            return data
    except json.JSONDecodeError:
        pass

    # Try to extract JSON from potentially corrupted text
    result = extract_and_fix_json(decoded)
    if result:
        return result

    return None


def extract_and_fix_json(text: str) -> Optional[dict]:
    """
    Extract and fix JSON from text that may contain multiple JSON objects
    or be corrupted/incomplete.

    Args:
        text: Text containing JSON.

    Returns:
        Parsed dictionary or None.
    """
    if not text:
        return None

    text = text.strip()

    # Try to find JSON object boundaries
    # Find first {
    start = text.find('{')
    if start == -1:
        return None

    # Find last }
    end = text.rfind('}')
    if end == -1:
        return None

    # Extract potential JSON
    json_text = text[start:end + 1]

    # Try to fix common issues
    json_text = fix_common_issues(json_text)

    try:
        data = json.loads(json_text)
        if isinstance(data, dict):
            return data
    except json.JSONDecodeError:
        pass

    # Try removing trailing commas
    json_text = re.sub(r',\s*([}\]])', r'\1', json_text)

    try:
        data = json.loads(json_text)
        if isinstance(data, dict):
            return data
    except json.JSONDecodeError:
        pass

    # Try to fix quotes around property names
    json_text = re.sub(r'([{,]\s*)([a-zA-Z_][a-zA-Z0-9_]*)\s*:', r'\1"\2":', json_text)

    try:
        data = json.loads(json_text)
        if isinstance(data, dict):
            return data
    except json.JSONDecodeError:
        pass

    return None


def fix_common_issues(text: str) -> str:
    """Fix common JSON formatting issues."""
    # Remove any content before first {
    first_brace = text.find('{')
    if first_brace > 0:
        text = text[first_brace:]

    # Remove any content after last }
    last_brace = text.rfind('}')
    if last_brace > 0:
        text = text[:last_brace + 1]

    return text


def check_required_fields(data: dict, required: list[str]) -> tuple[bool, Optional[str]]:
    """Check if dictionary has required fields."""
    missing = [field for field in required if field not in data]
    if missing:
        return False, f"Missing required fields: {', '.join(missing)}"
    return True, None
