from datetime import datetime, timezone
import logging

logger = logging.getLogger(__name__)

class TelemetryRepository:
    def __init__(self, database):
        self._collection = database.telemetry

    async def create(self, client_id: str, device_id: str, payload):
        document = payload.model_dump()
        logger.info(f"MQTT document: {document}")

        document.update({
            "client_id": client_id,
            "device_id": device_id,
            "received_at": datetime.now(timezone.utc),
        })
        await self._collection.insert_one(document)
        return document

    async def latest(self, client_id: str, device_id: str):
        cursor = self._collection.find(
            {"client_id": client_id, "device_id": device_id},
            {"_id": 0},
        ).sort([("received_at", -1)]).limit(1)
        records = await cursor.to_list(length=1)
        return records[0] if records else None
