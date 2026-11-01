import logging

logger = logging.getLogger(__name__)

class TelemetryService:
    def __init__(self, device_repository, telemetry_repository):
        self._devices = device_repository
        self._telemetry = telemetry_repository

    async def ingest(self, client_id: str, device_id: str, payload):
        # Ignore telemetry from unregistered devices; a client cannot create devices by publishing MQTT.
        # device = await self._devices.get_by_client_and_device(client_id, device_id)
        device = await self._devices.get_by_device(device_id)
        logger.info(f"MQTT device: {device.get("client_id")}")
        if not device or not device.get("client_id"):
            return

        logger.info(f"MQTT device1: {device}")
        await self._telemetry.create(device.get("client_id"), device_id, payload)

    async def latest(self, client_id: str, device_id: str):
        if not await self._devices.get_by_client_and_device(client_id, device_id):
            return None
        return await self._telemetry.latest(client_id, device_id)
