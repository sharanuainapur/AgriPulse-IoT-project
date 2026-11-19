import logging

from pydantic import ValidationError

from app.core.config import get_settings
from app.schemas.calibration import CalibrationData, CalibrationUpsert
from app.utils.mqtt_topics import (
    CALIBRATION_REQUEST_ACTION,
    CALIBRATION_SAVE_ACTION,
    calibration_reply_topic,
)

logger = logging.getLogger(__name__)


class CalibrationService:
    def __init__(self, repository, mqtt=None):
        self._repository = repository
        self._mqtt = mqtt
        self._settings = get_settings()

    async def publish_calibration_command(self, client_id: str, device_id: str, calibration: str) -> str:
        """Publish a calibration command for the device to pick up. Returns the topic used."""
        topic = f"{self._settings.mqtt_topic_prefix}/{client_id}/{device_id}/cali/{calibration}"
        await self._mqtt.publish_json(topic, {"command": "CALIBRATE", "calibration": calibration})
        return topic

    async def handle_mqtt_message(
            self, client_id: str, device_id: str, calibration: str, action: str, payload: dict
    ) -> None:
        """Entry point for inbound calibration MQTT messages from devices."""
        if action == CALIBRATION_REQUEST_ACTION:
            await self.reply_with_stored_calibration(client_id, device_id, calibration)
        elif action == CALIBRATION_SAVE_ACTION:
            await self.save_from_device(client_id, device_id, calibration, payload)

    async def reply_with_stored_calibration(self, client_id: str, device_id: str, calibration: str) -> None:
        """Fetch the calibration from MongoDB and publish it back to the requesting device."""
        doc = await self._repository.get(client_id, device_id, calibration)
        if doc is None:
            reply = {"status": "not_found", "calibration": calibration}
        else:
            reply = {"status": "ok", "calibration": calibration, "data": doc["data"]}

        topic = calibration_reply_topic(client_id, device_id, calibration, self._settings.mqtt_topic_prefix)
        await self._mqtt.publish_json(topic, reply)
        logger.info("Calibration %s sent to %s/%s (%s)", calibration, client_id, device_id, reply["status"])

    async def save_from_device(self, client_id: str, device_id: str, calibration: str, payload: dict) -> None:
        """Validate calibration data sent by a device and store it in MongoDB."""
        # Accept either {"dry_val":..,"wet_val":..} or {"data": {"dry_val":..,"wet_val":..}}.
        data = payload.get("data", payload)
        try:
            model = CalibrationUpsert(
                client_id=client_id,
                device_id=device_id,
                calibration=calibration,
                data=CalibrationData.model_validate(data),
            )
        except ValidationError:
            logger.warning(
                "Ignoring invalid calibration data from %s/%s for %s: %s",
                client_id, device_id, calibration, payload,
            )
            return
        await self.save(model)
        logger.info("Calibration %s stored for %s/%s", calibration, client_id, device_id)

    async def save(self, payload: CalibrationUpsert):
        """Create or update the calibration identified by (client_id, device_id, calibration)."""
        return await self._repository.upsert(
            payload.client_id,
            payload.device_id,
            payload.calibration,
            payload.data.dry_val,
            payload.data.wet_val,
        )

    async def get(self, client_id: str, device_id: str, calibration: str):
        doc = await self._repository.get(client_id, device_id, calibration)
        if doc is None:
            raise LookupError("Calibration not found")
        return doc

    async def list_by_device(self, client_id: str, device_id: str):
        return await self._repository.list_by_device(client_id, device_id)

    async def delete(self, client_id: str, device_id: str, calibration: str) -> None:
        if not await self._repository.delete(client_id, device_id, calibration):
            raise LookupError("Calibration not found")
