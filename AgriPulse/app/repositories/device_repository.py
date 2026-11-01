from datetime import datetime, timezone
from pymongo.errors import DuplicateKeyError
import logging

logger = logging.getLogger(__name__)

class DeviceRepository:
    def __init__(self, database):
        self._collection = database.devices

    async def create(self, client_id: str, device_id: str, name: str, location: str | None):
        now = datetime.now(timezone.utc)
        document = {"client_id": client_id, "device_id": device_id, "name": name,
                    "location": location, "created_at": now, "updated_at": now, "mode": False}
        try:
            await self._collection.insert_one(document)
        except DuplicateKeyError as exc:
            raise ValueError("Device ID already exists for this client") from exc
        return document

    from datetime import datetime, timezone
    from pymongo.errors import DuplicateKeyError

    async def upsert(self, device_id: str, name: str, location: str | None):
        now = datetime.now(timezone.utc)

        try:
            await self._collection.update_one(
                {"device_id": device_id},
                {
                    # "$set": document,
                    "$set": {
                        "device_id": device_id,
                        "name": name,
                        "location": location,
                        "updated_at": now,
                    },
                    "$setOnInsert": {
                        "created_at": now,
                    },
                },
                upsert=True,
            )
        except DuplicateKeyError as exc:
            raise ValueError(
                "Device ID already exists for this client"
            ) from exc

        return await self._collection.find_one(
            {"device_id": device_id},
            {"_id": 0},
        )

    async def upsert_device(self, device_id: str, **fields):
        now = datetime.now(timezone.utc)

        logger.info("---- device_id %s", device_id)
        logger.info("device_id %s", fields)
        update_fields = {
            **fields,
            "updated_at": now,
        }

        try:
            await self._collection.update_one(
                {"device_id": device_id},
                {
                    "$set": update_fields,
                    "$setOnInsert": {
                        "device_id": device_id,
                        "created_at": now,
                    },
                },
                upsert=True,
            )
        except DuplicateKeyError as exc:
            raise ValueError(
                "Device ID already exists for this client"
            ) from exc

        return await self._collection.find_one(
            {"device_id": device_id},
            {"_id": 0},
        )

    async def mapping(self, device_id: str, client_id: str):
        now = datetime.now(timezone.utc)

        try:
            await self._collection.update_one(
                {"device_id": device_id},
                {
                    "$set": {
                        "client_id": client_id,
                        "updated_at": now,
                    },
                    "$setOnInsert": {
                        "device_id": device_id,
                        "created_at": now,
                    },
                },
                upsert=True,
            )
        except DuplicateKeyError as exc:
            raise ValueError(
                "Device mapping already exists"
            ) from exc

        return await self._collection.find_one(
            {"device_id": device_id},
            {"_id": 0},
        )

    async def update_mode(self, device_id: str, mode: str):
        now = datetime.now(timezone.utc)

        try:
            await self._collection.update_one(
                {"device_id": device_id},
                {
                    "$set": {
                        "mode": mode,
                        "updated_at": now,
                    },
                },
                upsert=True,
            )
        except DuplicateKeyError as exc:
            raise ValueError(
                "Device mapping already exists"
            ) from exc

        return await self._collection.find_one(
            {"device_id": device_id},
            {"_id": 0},
        )

    async def get_by_client_and_device(self, client_id: str, device_id: str):
        return await self._collection.find_one({"client_id": client_id, "device_id": device_id}, {"_id": 0})

    async def get_by_device(self, device_id: str):
        return await self._collection.find_one({"device_id": device_id}, {"_id": 0})

    async def list_by_client(self, client_id: str):
        # cursor = self._collection.find({"client_id": client_id}, {"_id": 0}).sort("created_at", -1).limit(500)
        cursor = self._collection.find({}, {"_id": 0}).sort("created_at", -1).limit(500)
        return await cursor.to_list(length=500)
