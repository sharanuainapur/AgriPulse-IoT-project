from redis.asyncio import Redis
from app.core.config import Settings


class RedisManager:
    def __init__(self, settings: Settings):
        self._url = settings.redis_url
        self.client: Redis | None = None

    async def connect(self) -> None:
        self.client = Redis.from_url(self._url, decode_responses=True, socket_connect_timeout=3,
                                     socket_timeout=3, health_check_interval=30)
        await self.client.ping()

    async def ping(self) -> bool:
        try:
            return bool(self.client and await self.client.ping())
        except Exception:
            return False

    async def close(self) -> None:
        if self.client is not None:
            await self.client.aclose()
            self.client = None
