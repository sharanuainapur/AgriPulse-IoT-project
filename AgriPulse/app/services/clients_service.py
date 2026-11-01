from app.core.config import get_settings


class ClientsService:
    def __init__(self, repository, mqtt):
        self._repository = repository
        self._mqtt = mqtt
        self._settings = get_settings()

    async def register(self, payload):
        client = await self._repository.upsert(payload.client_id, payload.phone, payload.name)
        # Global wildcard subscriptions are already active; device-specific subscriptions are explicit too.
        # if self._mqtt.is_connected:
        #     await self._mqtt.subscribe_device(client_id, payload.device_id)
        return client

    async def list(self) -> str:
        clients = await self._repository.list()
        return clients

    # async def publish_motor_command(self, client_id: str, device_id: str, state: str) -> str:
    #     topic = f"{self._settings.mqtt_topic_prefix}/{client_id}/{device_id}/command"
    #     await self._mqtt.publish_json(topic, {"command": "SET_MOTOR", "state": state})
    #     return topic
