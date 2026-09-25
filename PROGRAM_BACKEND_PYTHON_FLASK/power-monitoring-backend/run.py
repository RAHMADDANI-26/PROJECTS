#!/usr/bin/env python3
"""
Power Monitoring Backend - Entry Point.

Subscribes to MQTT topics from ESP32 gateway and writes data to InfluxDB Cloud.
No REST API or HTTP server is provided.
"""

import signal
import sys
from typing import Optional

from app.config import get_influxdb_config, get_mqtt_config
from app.influx.writer import InfluxDBWriter
from app.mqtt.client import MQTTClient
from app.utils.logger import get_logger, setup_logging

# Global logger instance
logger: Optional[object] = None

# Global instances for graceful shutdown
mqtt_client: Optional[MQTTClient] = None
influx_writer: Optional[InfluxDBWriter] = None


def signal_handler(signum: int, frame) -> None:
    """Handle shutdown signals gracefully."""
    sig_name = signal.Signals(signum).name
    logger.info(f"Received {sig_name}, shutting down...")
    shutdown()


def shutdown() -> None:
    """Clean up resources and exit."""
    global mqtt_client, influx_writer

    logger.info("Cleaning up resources...")

    if mqtt_client is not None:
        try:
            mqtt_client.stop()
        except Exception as e:
            logger.error(f"Error stopping MQTT client: {e}")

    if influx_writer is not None:
        try:
            influx_writer.disconnect()
        except Exception as e:
            logger.error(f"Error disconnecting InfluxDB: {e}")

    logger.info("Shutdown complete")
    sys.exit(0)


def main() -> None:
    """Main entry point for the power monitoring backend."""
    global logger, mqtt_client, influx_writer

    # Setup logging
    setup_logging()
    logger = get_logger("power_monitoring")

    logger.info("=" * 60)
    logger.info("Power Monitoring Backend Starting")
    logger.info("=" * 60)

    # Load configuration
    try:
        mqtt_config = get_mqtt_config()
        influx_config = get_influxdb_config()
        logger.info("Configuration loaded successfully")
    except ValueError as e:
        logger.error(f"Configuration error: {e}")
        logger.error("Please ensure all required environment variables are set in .env file")
        sys.exit(1)

    # Initialize InfluxDB writer
    try:
        influx_writer = InfluxDBWriter(influx_config)
        influx_writer.connect()
    except ConnectionError as e:
        logger.error(f"Failed to connect to InfluxDB: {e}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error initializing InfluxDB: {e}", exc_info=True)
        sys.exit(1)

    # Initialize MQTT client
    try:
        mqtt_client = MQTTClient(mqtt_config, influx_writer)
        mqtt_client.connect()
    except ConnectionError as e:
        logger.error(f"Failed to connect to MQTT broker: {e}")
        influx_writer.disconnect()
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error initializing MQTT: {e}", exc_info=True)
        influx_writer.disconnect()
        sys.exit(1)

    # Setup signal handlers for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    logger.info("All services initialized successfully")
    logger.info("Starting MQTT client loop...")

    # Start MQTT client loop (blocking)
    try:
        mqtt_client.start()
    except Exception as e:
        logger.error(f"Error in MQTT client loop: {e}", exc_info=True)
        shutdown()


if __name__ == "__main__":
    main()
