"""
Topic mapping module for MQTT subscriptions.

Defines literal topic mappings to data types and building names.
No wildcard patterns are used - all topics are exact literal strings.
"""

from dataclasses import dataclass
from enum import Enum
from typing import Dict, Optional


class DataType(Enum):
    """Types of data received from MQTT topics."""
    PANEL = "panel"
    OUTAGE_STATUS = "outage_status"
    HEARTBEAT = "heartbeat"


class GedungName(Enum):
    """Building identifiers for outage alerts."""
    A = "A"
    B = "B"


@dataclass(frozen=True)
class TopicInfo:
    """Information about a subscribed MQTT topic."""
    topic: str
    data_type: DataType
    gedung: Optional[GedungName] = None
    qos: int = 0


# Literal topic mappings - NO wildcards allowed
# Key: MQTT topic string
# Value: TopicInfo with data type and optional building name
TOPIC_MAPPING: Dict[str, TopicInfo] = {
    "power meter/data": TopicInfo(
        topic="power meter/data",
        data_type=DataType.PANEL,
        qos=0,
    ),
    "alert pemadaman/gedung": TopicInfo(
        topic="alert pemadaman/gedung",
        data_type=DataType.OUTAGE_STATUS,
        qos=0,
    ),
}


def get_topic_info(topic: str) -> Optional[TopicInfo]:
    """
    Get topic information for a given MQTT topic.

    Args:
        topic: The MQTT topic string to look up.

    Returns:
        TopicInfo if topic is known, None otherwise.
    """
    return TOPIC_MAPPING.get(topic)


def get_all_topics() -> list[str]:
    """Get list of all subscribed MQTT topics."""
    return list(TOPIC_MAPPING.keys())


def get_topic_qos(topic: str) -> int:
    """Get QoS level for a topic."""
    info = get_topic_info(topic)
    return info.qos if info else 0
