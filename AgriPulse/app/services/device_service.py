from app.core.config import get_settings


class DeviceService:
    def __init__(self, repository, mqtt):
        self._repository = repository
        self._mqtt = mqtt
        self._settings = get_settings()

    async def register(self, payload):
        # device = await self._repository.upsert(payload.device_id, payload.name, payload.location)
        device = await self._repository.upsert_device(
            payload.device_id,
            name=payload.name,
            location=payload.location,
            mode="AUTO",
            state="OFF")

        # Global wildcard subscriptions are already active; device-specific subscriptions are explicit too.
        if self._mqtt.is_connected:
            # await self._mqtt.subscribe_device(client_id, payload.device_id)
            await self._mqtt.subscribe_device("client_id", payload.device_id)
        return device

    async def update_mode(self, payload, publish :bool):
        device = await self._repository.update_mode(payload.device_id, payload.mode)
        # Global wildcard subscriptions are already active; device-specific subscriptions are explicit too.
        if self._mqtt.is_connected and publish:
            # await self._mqtt.subscribe_device(client_id, payload.device_id)
            await self._mqtt.subscribe_device("client_id", payload.device_id)
            topic = f"{self._settings.mqtt_topic_prefix}/{payload.device_id}/command"
            await self._mqtt.publish_json(topic, {"mode": payload.mode, "pump": "OFF"})
        return device

    async def mapping(self, payload):
        device = await self._repository.mapping(payload.device_id, payload.client_id)
        # Global wildcard subscriptions are already active; device-specific subscriptions are explicit too.
        if self._mqtt.is_connected:
            # await self._mqtt.subscribe_device(client_id, payload.device_id)
            await self._mqtt.subscribe_device("client_id", payload.device_id)
        return device

    async def publish_motor_command(self, device_id: str, state: str) -> str:
        topic = f"{self._settings.mqtt_topic_prefix}/{device_id}/command"
        await self._mqtt.publish_json(topic, {"mode": "MANUAL", "pump": state})
        return topic
