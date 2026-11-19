from datetime import datetime, timezone
from pymongo import ReturnDocument
from pymongo.errors import DuplicateKeyError


class CalibrationRepository:
    def __init__(self, database):
        self._collection = database.calibration

    @staticmethod
    def _key(client_id: str, device_id: str, calibration: str) -> dict:
        return {"client_id": client_id, "device_id": device_id, "calibration": calibration}

    async def upsert(self, client_id: str, device_id: str, calibration: str, dry_val: float, wet_val: float):
        """Create the calibration if it doesn't exist, otherwise update its data."""
        now = datetime.now(timezone.utc)
        try:
            return await self._collection.find_one_and_update(
                self._key(client_id, device_id, calibration),
                {
                    "$set": {
                        "data": {"dry_val": dry_val, "wet_val": wet_val},
                        "updated_at": now,
                    },
                    "$setOnInsert": {"created_at": now},
                },
                upsert=True,
                projection={"_id": 0},
                return_document=ReturnDocument.AFTER,
            )
        except DuplicateKeyError as exc:
            # Only possible on a race between two concurrent first-time upserts.
            raise ValueError("Calibration already exists for this device") from exc

    async def get(self, client_id: str, device_id: str, calibration: str):
        return await self._collection.find_one(self._key(client_id, device_id, calibration), {"_id": 0})

    async def list_by_device(self, client_id: str, device_id: str):
        cursor = self._collection.find(
            {"client_id": client_id, "device_id": device_id}, {"_id": 0}
        ).sort("calibration", 1).limit(500)
        return await cursor.to_list(length=500)

    async def delete(self, client_id: str, device_id: str, calibration: str) -> bool:
        result = await self._collection.delete_one(self._key(client_id, device_id, calibration))
        return result.deleted_count > 0
