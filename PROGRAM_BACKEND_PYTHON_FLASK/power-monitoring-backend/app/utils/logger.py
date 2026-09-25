"""
Logging utilities for Power Monitoring Backend.

Provides structured logging configuration using Python's standard logging module.
"""

import logging
import sys
from typing import Optional


# Default logger name
DEFAULT_LOGGER_NAME = "power_monitoring"


def setup_logging(
    level: int = logging.INFO,
    format_string: Optional[str] = None,
    log_to_file: bool = False,
    log_file: str = "power_monitoring.log",
) -> None:
    """
    Configure logging for the application.

    Args:
        level: Logging level (default: INFO).
        format_string: Custom log format string. If None, uses default.
        log_to_file: Whether to also log to a file.
        log_file: Path to log file if log_to_file is True.
    """
    if format_string is None:
        format_string = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"

    handlers: list[logging.Handler] = [logging.StreamHandler(sys.stdout)]

    if log_to_file:
        file_handler = logging.FileHandler(log_file)
        file_handler.setFormatter(logging.Formatter(format_string))
        handlers.append(file_handler)

    logging.basicConfig(
        level=level,
        format=format_string,
        handlers=handlers,
        force=True,
    )

    # Reduce noise from third-party libraries
    logging.getLogger("paho").setLevel(logging.WARNING)
    logging.getLogger("urllib3").setLevel(logging.WARNING)


def get_logger(name: Optional[str] = None) -> logging.Logger:
    """
    Get a logger instance.

    Args:
        name: Logger name. If None, uses the default logger name.

    Returns:
        Logger instance.
    """
    if name is None:
        name = DEFAULT_LOGGER_NAME
    return logging.getLogger(name)
