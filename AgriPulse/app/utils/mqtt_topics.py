DEFAULT_TOPIC_PREFIX = "agripulse"
VALID_TOPIC_KINDS = frozenset({"telemetry", "state"})


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
