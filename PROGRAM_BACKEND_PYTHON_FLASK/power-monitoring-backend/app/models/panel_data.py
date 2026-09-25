"""
Panel data model for DIRIS A-20 via ESP32 Modbus.

Matches actual MQTT JSON format from gateway.
"""

from dataclasses import dataclass
import time


@dataclass(frozen=True)
class PanelData:
    """
    Panel power meter data from DIRIS A-20.

    Actual MQTT JSON format:
    {
        "device": "DIRIS_A20",
        "panel_id": 1,
        "message_id": 77,
        "timestamp": 385000,
        "voltage": {"v12": 402.58, "v23": 405.24, "v31": 405.92, "v1": 231.49, "v2": 233.21, "v3": 236.40},
        "frequency": 50.05,
        "current": {"i1": 16.965, "i2": 15.345, "i3": 6.615, "in": 7.065},
        "power": {"total": 84.9, "kvar": -10.8, "kva": 85.6, "l1": 37.2, "l2": 34.3, "l3": 13.3},
        "pf": 0.991,
        "thd_voltage": {"v12": 2.6, "v23": 2.6, "v31": 2.5, "v1": 3.8, "v2": 3.8, "v3": 3.6},
        "thd_current": {"i1": 23.0, "i2": 26.3, "i3": 53.4},
        "energy": 6173491.9
    }
    """
    raw_data: dict
    timestamp: int

    @classmethod
    def from_dict(cls, data: dict) -> "PanelData":
        """
        Create PanelData from MQTT JSON payload.

        Args:
            data: Dictionary from MQTT payload.

        Returns:
            PanelData instance.
        """
        current_time = int(time.time())

        ts = data.get("timestamp") or data.get("ts") or data.get("t")

        if ts is None:
            ts = current_time
        else:
            ts = int(ts)
            # If timestamp is very old or too far in future, use server time
            if ts < 1577836800 or ts > current_time + 86400:
                ts = current_time

        return cls(
            raw_data=data,
            timestamp=ts,
        )
