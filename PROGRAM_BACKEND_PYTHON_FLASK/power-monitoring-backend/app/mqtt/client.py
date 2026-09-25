"""
MQTT client module for Power Monitoring Backend.

Provides MQTT client with auto-reconnect functionality for subscribing
to ESP32 gateway topics and forwarding data to InfluxDB.
"""

import time
from typing import Optional

import paho.mqtt.client as mqtt
from paho.mqtt.enums import MQTTErrorCode

from app.config import MQTTConfig
from app.influx.writer import InfluxDBWriter
from app.models.panel_data import PanelData
from app.models.alert_data import AlertData
from app.mqtt.topics import (
    DataType,
    get_all_topics,
    get_topic_info,
    get_topic_qos,
)
from app.utils.logger import get_logger
from app.utils.validators import validate_json_payload

logger = get_logger(__name__)


class MQTTClient:
    """
    MQTT client for subscribing to power monitoring topics.

    Handles connection, reconnection, and message routing to InfluxDB writer.

    Args:
        config: MQTT broker configuration.
        influx_writer: InfluxDB writer instance for data persistence.
    """

    def __init__(self, config: MQTTConfig, influx_writer: InfluxDBWriter) -> None:
        self.config = config
        self.influx_writer = influx_writer
        self._client: Optional[mqtt.Client] = None
        self._connected = False
        self._should_run = False

    def _on_connect(
        self,
        client: mqtt.Client,
        userdata: dict,
        flags: dict,
        rc: int,
        properties: Optional[mqtt.Properties] = None,
    ) -> None:
        """Callback when connection is established."""
        if rc == MQTTErrorCode.MQTT_ERR_SUCCESS:
            self._connected = True
            logger.info(f"Connected to MQTT broker at {self.config.host}:{self.config.port}")

            # Subscribe to all configured topics
            topics = get_all_topics()
            for topic in topics:
                qos = get_topic_qos(topic)
                result = client.subscribe(topic, qos)
                if result[0] == MQTTErrorCode.MQTT_ERR_SUCCESS:
                    logger.info(f"Subscribed to topic '{topic}' with QoS {qos}")
                else:
                    logger.error(f"Failed to subscribe to '{topic}': {result}")
        else:
            error_messages = {
                1: "Connection refused: Protocol version mismatch",
                2: "Connection refused: Client ID rejected",
                3: "Connection refused: Server unavailable",
                4: "Connection refused: Bad username or password",
                5: "Connection refused: Not authorized",
                6: "Connection refused: Rate limited",
            }
            error_msg = error_messages.get(rc, f"Unknown error code {rc}")
            logger.error(f"MQTT connection failed: {error_msg}")
            logger.error("Check MQTT_HOST, MQTT_USERNAME, MQTT_PASSWORD in .env file")
            self._connected = False

    def _on_disconnect(
        self,
        client: mqtt.Client,
        userdata: dict,
        rc: int,
        properties: Optional[mqtt.Properties] = None,
    ) -> None:
        """Callback when disconnection occurs."""
        self._connected = False
        if rc == 0:
            logger.info("Disconnected from MQTT broker (clean disconnect)")
        else:
            logger.warning(f"Disconnected unexpectedly with code: {rc}. Will attempt reconnect.")

    def _on_message(
        self,
        client: mqtt.Client,
        userdata: dict,
        message: mqtt.MQTTMessage,
        properties: Optional[mqtt.Properties] = None,
    ) -> None:
        """
        Callback when a message is received.

        Routes the message to the appropriate handler based on topic.
        """
        topic = message.topic
        payload = message.payload

        logger.debug(f"Received message on topic '{topic}'")

        # Get topic info to determine data type
        topic_info = get_topic_info(topic)
        if topic_info is None:
            logger.warning(f"Unknown topic '{topic}', ignoring message")
            return

        # Parse payload
        data = validate_json_payload(payload)
        if data is None:
            return

        try:
            if topic_info.data_type == DataType.PANEL:
                self._handle_panel_data(data)
            elif topic_info.data_type == DataType.OUTAGE_STATUS:
                self._handle_alert_data(data)
            else:
                logger.debug(f"Unhandled data type '{topic_info.data_type}' for topic '{topic}'")
        except Exception as e:
            logger.error(f"Error processing message from '{topic}': {e}", exc_info=True)

    def _handle_panel_data(self, data: dict) -> None:
        """Handle panel power meter data (DIRIS A-20 format)."""
        try:
            panel_data = PanelData.from_dict(data)
            self.influx_writer.write_panel_data(panel_data)
            logger.info("Panel data received and written to InfluxDB")
        except Exception as e:
            logger.error(f"Invalid panel data: {e}")

    def _handle_alert_data(self, data: dict) -> None:
        """Handle power outage alert data."""
        try:
            alert_data = AlertData.from_dict(data)
            self.influx_writer.write_alert_data(alert_data)
            logger.info("Alert data received and written to InfluxDB")
        except Exception as e:
            logger.error(f"Invalid alert data: {e}")

    def connect(self) -> None:
        """Establish connection to MQTT broker."""
        # Create client with or without authentication
        if self.config.username and self.config.password:
            self._client = mqtt.Client(
                client_id=self.config.client_id,
                clean_session=self.config.clean_session,
            )
            self._client.username_pw_set(
                username=self.config.username,
                password=self.config.password,
            )
        else:
            self._client = mqtt.Client(
                client_id=self.config.client_id,
                clean_session=self.config.clean_session,
            )

        # Set callbacks
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message

        # Connect with retry
        max_retries = 5
        retry_delay = 5

        for attempt in range(max_retries):
            try:
                logger.info(
                    f"Connecting to MQTT broker at {self.config.host}:{self.config.port} "
                    f"(attempt {attempt + 1}/{max_retries})..."
                )
                self._client.connect(
                    host=self.config.host,
                    port=self.config.port,
                    keepalive=self.config.keepalive,
                )
                logger.info("Successfully connected to MQTT broker")
                return
            except Exception as e:
                logger.error(f"Failed to connect: {e}")
                if attempt < max_retries - 1:
                    logger.info(f"Retrying in {retry_delay} seconds...")
                    time.sleep(retry_delay)
                    retry_delay *= 2  # Exponential backoff

        raise ConnectionError(f"Failed to connect to MQTT broker after {max_retries} attempts")

    def start(self) -> None:
        """
        Start the MQTT client loop.

        This method blocks and should be run in the main thread.
        """
        if self._client is None:
            raise RuntimeError("Client not connected. Call connect() first.")

        self._should_run = True
        self._client.loop_start()
        logger.info("MQTT client loop started")

        reconnect_delay = 5  # seconds between reconnect attempts

        try:
            while self._should_run:
                time.sleep(1)
                if not self._connected:
                    logger.warning(f"Connection lost, attempting reconnect in {reconnect_delay}s...")
                    time.sleep(reconnect_delay)
                    self._client.reconnect()
                    reconnect_delay = min(reconnect_delay * 2, 60)  # Max 60s delay
        except KeyboardInterrupt:
            logger.info("Received interrupt signal")
        finally:
            self.stop()

    def stop(self) -> None:
        """Stop the MQTT client and disconnect."""
        self._should_run = False
        if self._client is not None:
            self._client.loop_stop()
            self._client.disconnect()
            logger.info("MQTT client stopped")
