"""
InfluxDB schema definitions for Power Monitoring Backend.

Defines measurement names, tags, and fields for storing power monitoring data.
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import Dict, List


class Measurement(Enum):
    """InfluxDB measurement names."""
    PANEL_3PHASE = "panel_3phase"
    OUTAGE_STATUS = "outage_status"


# Measurement schemas
@dataclass(frozen=True)
class MeasurementSchema:
    """Schema definition for an InfluxDB measurement."""
    name: str
    tags: List[str]
    required_fields: List[str]


PANEL_3PHASE_SCHEMA = MeasurementSchema(
    name="panel_3phase",
    tags=["phase"],
    required_fields=["voltage", "current", "power", "power_factor", "energy"],
)

OUTAGE_STATUS_SCHEMA = MeasurementSchema(
    name="outage_status",
    tags=["gedung"],
    required_fields=["status", "battery"],
)

# Registry of all measurements
MEASUREMENTS: Dict[str, MeasurementSchema] = {
    Measurement.PANEL_3PHASE.value: PANEL_3PHASE_SCHEMA,
    Measurement.OUTAGE_STATUS.value: OUTAGE_STATUS_SCHEMA,
}
