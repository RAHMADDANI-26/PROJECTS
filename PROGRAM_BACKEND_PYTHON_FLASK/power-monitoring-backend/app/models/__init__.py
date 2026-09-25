"""
Data models module for Power Monitoring Backend.

Provides dataclasses for validating and transforming MQTT payload data.
"""

from app.models.panel_data import PanelData
from app.models.alert_data import AlertData

__all__ = ["PanelData", "AlertData"]
