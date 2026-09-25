"""
Configuration module for Power Monitoring Backend.

Loads configuration from environment variables using python-dotenv.
"""

import uuid
from dataclasses import dataclass
from os import environ
from pathlib import Path
from typing import Optional

from dotenv import load_dotenv

# Load .env file if present
_env_path = Path(__file__).parent.parent / ".env"
if _env_path.exists():
    load_dotenv(dotenv_path=_env_path)


@dataclass(frozen=True)
class MQTTConfig:
    """MQTT broker configuration."""
    host: str
    port: int
    username: Optional[str] = None
    password: Optional[str] = None
    client_id: Optional[str] = None
    keepalive: int = 60
    clean_session: bool = True

    @classmethod
    def from_env(cls) -> "MQTTConfig":
        """Create MQTTConfig from environment variables."""
        # Generate random client_id if not provided
        client_id = environ.get("MQTT_CLIENT_ID")
        if not client_id:
            client_id = f"power_backend_{uuid.uuid4().hex[:8]}"

        return cls(
            host=environ["MQTT_HOST"],
            port=int(environ.get("MQTT_PORT", "1883")),
            username=environ.get("MQTT_USERNAME") or None,
            password=environ.get("MQTT_PASSWORD") or None,
            client_id=client_id,
            keepalive=int(environ.get("MQTT_KEEPALIVE", "60")),
            clean_session=environ.get("MQTT_CLEAN_SESSION", "true").lower() == "true",
        )


@dataclass(frozen=True)
class InfluxDBConfig:
    """InfluxDB configuration."""
    url: str
    token: str
    bucket: str
    org: Optional[str] = None
    timeout_ms: int = 10_000
    retry_interval: int = 5

    @classmethod
    def from_env(cls) -> "InfluxDBConfig":
        """Create InfluxDBConfig from environment variables."""
        return cls(
            url=environ["INFLUXDB_URL"],
            token=environ["INFLUXDB_TOKEN"],
            bucket=environ["INFLUXDB_BUCKET"],
            org=environ.get("INFLUXDB_ORG") or None,
            timeout_ms=int(environ.get("INFLUXDB_TIMEOUT_MS", "10000")),
            retry_interval=int(environ.get("INFLUXDB_RETRY_INTERVAL", "5")),
        )


def get_mqtt_config() -> MQTTConfig:
    """Get MQTT configuration from environment."""
    required_vars = ["MQTT_HOST"]
    missing = [v for v in required_vars if v not in environ]
    if missing:
        raise ValueError(f"Missing required environment variables: {', '.join(missing)}")
    return MQTTConfig.from_env()


def get_influxdb_config() -> InfluxDBConfig:
    """Get InfluxDB configuration from environment."""
    required_vars = ["INFLUXDB_URL", "INFLUXDB_TOKEN", "INFLUXDB_BUCKET"]
    missing = [v for v in required_vars if v not in environ]
    if missing:
        raise ValueError(f"Missing required environment variables: {', '.join(missing)}")
    return InfluxDBConfig.from_env()
