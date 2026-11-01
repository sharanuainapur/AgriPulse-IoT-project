from datetime import datetime, timezone
from pymongo.errors import DuplicateKeyError


class ClientsRepository:
    def __init__(self, database):
        self._collection = database.clients

    async def create(self, client_id: str, phone: str, name: str):
        now = datetime.now(timezone.utc)
        document = {"client_id": client_id, "phone": phone, "name": name,
                    "created_at": now, "updated_at": now}
        try:
            await self._collection.insert_one(document)
        except DuplicateKeyError as exc:
            raise ValueError("Device ID already exists for this client") from exc
        return document

    async def upsert(self, client_id: str, phone: str, name: str):
        now = datetime.now(timezone.utc)

        document = {
            "client_id": client_id,
            "phone": phone,
            "name": name,
            "updated_at": now,
        }

        try:
            await self._collection.update_one(
                {"client_id": client_id},
                {
                    "$set": document,
                    "$setOnInsert": {"created_at": now},
                },
                upsert=True,
            )
        except DuplicateKeyError as exc:
            raise ValueError(
                "Device ID already exists for this client"
            ) from exc

        return await self._collection.find_one(
            {"client_id": client_id}
        )

    async def get_by_client_id(self, client_id: str):
        return await self._collection.find_one({"client_id": client_id}, {"_id": 0})

    async def list(self):
        cursor = self._collection.find({}, {"_id": 0}).sort("created_at", -1).limit(500)
        return await cursor.to_list(length=500)
