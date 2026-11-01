import pytest
from app.utils.mqtt_topics import parse_device_topic


def test_parse_telemetry_topic():
    assert parse_device_topic("agripulse/farm-1/pump-1/telemetry") == {
        "client_id": "farm-1", "device_id": "pump-1", "kind": "telemetry"
    }


def test_parse_state_topic():
    assert parse_device_topic("agripulse/farm-1/pump-1/state")["kind"] == "state"


@pytest.mark.parametrize("topic", [
    "bad/farm/pump/telemetry",
    "agripulse/farm/pump/command",
    "agripulse/farm/pump",
    "agripulse//pump/state",
])
def test_reject_invalid_topics(topic):
    with pytest.raises(ValueError):
        parse_device_topic(topic)
