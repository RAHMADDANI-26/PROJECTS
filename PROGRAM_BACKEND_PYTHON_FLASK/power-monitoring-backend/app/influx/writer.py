"""
InfluxDB writer module for Power Monitoring Backend.

Provides functionality to write panel and outage data to InfluxDB Cloud.
"""

import time
from typing import Optional

from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import WriteApi

from app.config import InfluxDBConfig
from app.models.panel_data import PanelData
from app.models.alert_data import AlertData
from app.utils.logger import get_logger

logger = get_logger(__name__)


class InfluxDBWriter:
    """
    Writer for persisting power monitoring data to InfluxDB Cloud.

    Uses synchronous write API to ensure data consistency.

    Args:
        config: InfluxDB Cloud configuration.
    """

    def __init__(self, config: InfluxDBConfig) -> None:
        self.config = config
        self._client: Optional[InfluxDBClient] = None
        self._write_api: Optional[WriteApi] = None

    def connect(self) -> None:
        """Establish connection to InfluxDB."""
        max_retries = 5
        retry_delay = self.config.retry_interval

        # For InfluxDB 1.x compatibility, use empty org if not provided
        org = self.config.org or ""

        for attempt in range(max_retries):
            try:
                self._client = InfluxDBClient(
                    url=self.config.url,
                    token=self.config.token,
                    org=org,
                    timeout=self.config.timeout_ms,
                )

                # Test connection by reading ping
                self._client.ping()
                self._write_api = self._client.write_api()

                org_display = self.config.org if self.config.org else "(default)"
                logger.info(
                    f"Connected to InfluxDB at {self.config.url}, "
                    f"org={org_display}, bucket={self.config.bucket}"
                )
                return
            except Exception as e:
                logger.error(f"Failed to connect to InfluxDB (attempt {attempt + 1}/{max_retries}): {e}")
                if attempt < max_retries - 1:
                    logger.info(f"Retrying in {retry_delay} seconds...")
                    time.sleep(retry_delay)
                    retry_delay *= 2  # Exponential backoff

        raise ConnectionError(
            f"Failed to connect to InfluxDB Cloud after {max_retries} attempts"
        )

    def disconnect(self) -> None:
        """Close connection to InfluxDB Cloud."""
        if self._write_api is not None:
            self._write_api.close()
            self._write_api = None
        if self._client is not None:
            self._client.close()
            self._client = None
        logger.info("Disconnected from InfluxDB Cloud")

    def _write_point(self, point: Point) -> bool:
        """
        Write a single point to InfluxDB.

        Args:
            point: InfluxDB Point to write.

        Returns:
            True if successful, False otherwise.
        """
        if self._write_api is None:
            logger.error("Write API not initialized. Call connect() first.")
            return False

        org = self.config.org or ""

        try:
            self._write_api.write(
                bucket=self.config.bucket,
                org=org,
                record=point,
            )
            return True
        except Exception as e:
            logger.error(f"InfluxDB write error: {e}", exc_info=True)
            return False

    def write_panel_data(self, data: PanelData) -> bool:
        """
        Write panel data to InfluxDB matching actual MQTT JSON format.

        Field order matches MQTT JSON structure exactly.

        Args:
            data: PanelData instance with raw data.

        Returns:
            True if successful, False otherwise.
        """
        raw = data.raw_data

        point = Point("panel_data")
        point = point.time(data.timestamp * 1_000_000_000)

        # === DEVICE INFO ===
        if "device" in raw:
            point = point.field("device", str(raw["device"]))
        if "panel_id" in raw:
            point = point.field("panel_id", int(raw["panel_id"]))
        if "message_id" in raw:
            point = point.field("message_id", int(raw["message_id"]))

        # === VOLTAGE (nested) ===
        voltage = raw.get("voltage", {})
        if "v12" in voltage:
            point = point.field("voltage_l12_v", float(voltage["v12"]))
        if "v23" in voltage:
            point = point.field("voltage_l23_v", float(voltage["v23"]))
        if "v31" in voltage:
            point = point.field("voltage_l31_v", float(voltage["v31"]))
        if "v1" in voltage:
            point = point.field("voltage_l1_v", float(voltage["v1"]))
        if "v2" in voltage:
            point = point.field("voltage_l2_v", float(voltage["v2"]))
        if "v3" in voltage:
            point = point.field("voltage_l3_v", float(voltage["v3"]))

        # === FREQUENCY ===
        if "frequency" in raw:
            point = point.field("frequency_hz", float(raw["frequency"]))

        # === CURRENT (nested) ===
        current = raw.get("current", {})
        if "i1" in current:
            point = point.field("current_l1_a", float(current["i1"]))
        if "i2" in current:
            point = point.field("current_l2_a", float(current["i2"]))
        if "i3" in current:
            point = point.field("current_l3_a", float(current["i3"]))
        if "in" in current:
            point = point.field("current_neutral_a", float(current["in"]))

        # === POWER (nested) ===
        power = raw.get("power", {})
        if "total" in power:
            point = point.field("power_active_total_w", float(power["total"]))
        if "kvar" in power:
            point = point.field("power_reactive_total_var", float(power["kvar"]))
        if "kva" in power:
            point = point.field("power_apparent_total_va", float(power["kva"]))
        if "l1" in power:
            point = point.field("power_active_l1_w", float(power["l1"]))
        if "l2" in power:
            point = point.field("power_active_l2_w", float(power["l2"]))
        if "l3" in power:
            point = point.field("power_active_l3_w", float(power["l3"]))

        # === POWER FACTOR ===
        if "pf" in raw:
            point = point.field("power_factor", float(raw["pf"]))

        # === THD VOLTAGE (nested) ===
        thd_v = raw.get("thd_voltage", {})
        if "v12" in thd_v:
            point = point.field("thd_voltage_l12_percent", float(thd_v["v12"]))
        if "v23" in thd_v:
            point = point.field("thd_voltage_l23_percent", float(thd_v["v23"]))
        if "v31" in thd_v:
            point = point.field("thd_voltage_l31_percent", float(thd_v["v31"]))
        if "v1" in thd_v:
            point = point.field("thd_voltage_l1_percent", float(thd_v["v1"]))
        if "v2" in thd_v:
            point = point.field("thd_voltage_l2_percent", float(thd_v["v2"]))
        if "v3" in thd_v:
            point = point.field("thd_voltage_l3_percent", float(thd_v["v3"]))

        # === THD CURRENT (nested) ===
        thd_i = raw.get("thd_current", {})
        if "i1" in thd_i:
            point = point.field("thd_current_l1_percent", float(thd_i["i1"]))
        if "i2" in thd_i:
            point = point.field("thd_current_l2_percent", float(thd_i["i2"]))
        if "i3" in thd_i:
            point = point.field("thd_current_l3_percent", float(thd_i["i3"]))

        # === ENERGY ===
        if "energy" in raw:
            point = point.field("energy_wh", float(raw["energy"]))

        success = self._write_point(point)
        if success:
            logger.info("Panel data written successfully")
        return success

    def write_alert_data(self, data: "AlertData") -> bool:
        """
        Write alert data to InfluxDB (measurement: alert_data).

        Fields match MQTT JSON format exactly.

        Args:
            data: AlertData instance with raw data.

        Returns:
            True if successful, False otherwise.
        """
        raw = data.raw_data

        point = Point("alert_data")
        point = point.time(data.timestamp * 1_000_000_000)

        # === DEVICE INFO ===
        if "device" in raw:
            point = point.field("device", str(raw["device"]))

        # === NODE/GEDUNG ===
        if "node" in raw:
            point = point.field("node", str(raw["node"]))

        # === STATUS ===
        if "status" in raw:
            point = point.field("status", str(raw["status"]))
        if "power_status" in raw:
            point = point.field("power_status", int(raw["power_status"]))

        # === BATTERY ===
        if "battery_percent" in raw:
            point = point.field("battery_percent", float(raw["battery_percent"]))

        success = self._write_point(point)
        if success:
            logger.info("Alert data written successfully")
        return success

    def __enter__(self) -> "InfluxDBWriter":
        """Context manager entry."""
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit."""
        self.disconnect()
