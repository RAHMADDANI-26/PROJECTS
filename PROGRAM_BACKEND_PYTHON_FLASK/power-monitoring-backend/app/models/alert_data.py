"""
Alert data model for power outage alerts.

Expected JSON format from MQTT:
{
    "device": "POWER_ALERT",
    "node": "GEDUNG_A",
    "status": "OFF",
    "power_status": 0,
    "battery_percent": 24.1,
    "timestamp": 547941
}
"""

from dataclasses import dataclass
import time


@dataclass(frozen=True)
class AlertData:
    """
    Power outage alert data.
    """
    raw_data: dict
    timestamp: int

    @classmethod
    def from_dict(cls, data: dict) -> "AlertData":
        """
        Create AlertData from MQTT JSON payload.

        Args:
            data: Dictionary from MQTT payload.

        Returns:
            AlertData instance.
        """
        current_time = int(time.time())

        ts = data.get("timestamp") or data.get("ts")

        if ts is None:
            ts = current_time
        else:
            ts = int(ts)
            if ts < 1577836800 or ts > current_time + 86400:
                ts = current_time

        return cls(
            raw_data=data,
            timestamp=ts,
        )
