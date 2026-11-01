from fastapi import APIRouter, Depends, Request
from app.core.dependencies import get_mongo, get_redis, get_mqtt

router = APIRouter()


@router.get("/live")
async def liveness():
    return {"status": "ok", "service": "AgriPulse"}


@router.get("/ready")
async def readiness(
    mongo=Depends(get_mongo),
    redis=Depends(get_redis),
    mqtt=Depends(get_mqtt),
):
    checks = {
        "mongodb": await mongo.ping(),
        "redis": await redis.ping(),
        "mqtt": mqtt.is_connected,
    }
    ready = all(checks.values())
    return {"status": "ready" if ready else "degraded", "checks": checks}
