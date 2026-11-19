DEFAULT_TOPIC_PREFIX = "agripulse"
VALID_TOPIC_KINDS = frozenset({"telemetry", "state"})

# Calibration topics: {prefix}/{client_id}/{device_id}/cali/{calibration}/{action}
#   get  -> device asks for its stored calibration (server replies on .../data)
#   save -> device sends calibration data to be stored
#   data -> server publishes stored calibration back to the device
CALIBRATION_SEGMENT = "cali"
CALIBRATION_REQUEST_ACTION = "get"
CALIBRATION_SAVE_ACTION = "save"
CALIBRATION_REPLY_ACTION = "data"
VALID_CALIBRATION_ACTIONS = frozenset({CALIBRATION_REQUEST_ACTION, CALIBRATION_SAVE_ACTION})


def calibration_subscription_topics(prefix: str = DEFAULT_TOPIC_PREFIX) -> list[str]:
    return [f"{prefix}/+/+/{CALIBRATION_SEGMENT}/+/{action}" for action in sorted(VALID_CALIBRATION_ACTIONS)]


def calibration_reply_topic(client_id: str, device_id: str, calibration: str,
                            prefix: str = DEFAULT_TOPIC_PREFIX) -> str:
    return f"{prefix}/{client_id}/{device_id}/{CALIBRATION_SEGMENT}/{calibration}/{CALIBRATION_REPLY_ACTION}"


def parse_calibration_topic(topic: str, prefix: str = DEFAULT_TOPIC_PREFIX) -> dict[str, str] | None:
    """Return topic parts for an inbound calibration topic, or None if it is not one."""
    parts = topic.split("/")
    if (
        len(parts) != 6
        or parts[0] != prefix
        or parts[3] != CALIBRATION_SEGMENT
        or parts[5] not in VALID_CALIBRATION_ACTIONS
        or not all(parts[1:3]) or not parts[4]
    ):
        return None
    return {
        "client_id": parts[1],
        "device_id": parts[2],
        "calibration": parts[4],
        "action": parts[5],
    }


def parse_device_topic(topic: str, prefix: str = DEFAULT_TOPIC_PREFIX) -> dict[str, str]:
    parts = topic.split("/")

    if len(parts) != 3 or parts[0] != prefix or parts[2] not in VALID_TOPIC_KINDS:
        raise ValueError("Invalid AgriPulse MQTT topic")

    # client_id, device_id = parts[1], parts[2]
    device_id = parts[1]

    if not device_id:
        raise ValueError("MQTT topic is missing client/device ID")

    return {
        # "client_id": client_id,
        "device_id": device_id,
        "kind": parts[2],
    }
