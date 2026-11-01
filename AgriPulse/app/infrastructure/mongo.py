from pymongo import AsyncMongoClient, ASCENDING, DESCENDING
from app.core.config import Settings


class MongoManager:
    def __init__(self, settings: Settings):
        self._settings = settings
        self.client: AsyncMongoClient | None = None
        self.database = None

    async def connect(self) -> None:
        self.client = AsyncMongoClient(self._settings.mongodb_uri, serverSelectionTimeoutMS=5000,
                                       maxPoolSize=50, minPoolSize=2)
        await self.client.admin.command("ping")
        self.database = self.client[self._settings.mongodb_database]
        await self.database.devices.create_index([("client_id", ASCENDING), ("device_id", ASCENDING)], unique=True)
        await self.database.telemetry.create_index(
            [("client_id", ASCENDING), ("device_id", ASCENDING), ("recorded_at", DESCENDING), ("_id", DESCENDING)]
        )

    async def ping(self) -> bool:
        try:
            if self.client is None:
                return False
            await self.client.admin.command("ping")
            return True
        except Exception:
            return False

    async def close(self) -> None:
        if self.client is not None:
            await self.client.close()
            self.client = None
