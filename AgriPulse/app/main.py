from contextlib import asynccontextmanager
import asyncio
import logging

from fastapi import FastAPI
from fastapi.responses import ORJSONResponse
from fastapi.middleware.cors import CORSMiddleware

from app.api.router import api_router
from app.core.config import get_settings
from app.infrastructure.mongo import MongoManager
from app.infrastructure.redis_client import RedisManager
from app.infrastructure.mqtt_client import MqttService
from app.repositories.clients_repository import ClientsRepository
from app.repositories.device_repository import DeviceRepository
from app.repositories.telemetry_repository import TelemetryRepository

logging.basicConfig(level=getattr(logging, get_settings().log_level.upper(), logging.INFO))
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings = get_settings()
    mongo = MongoManager(settings)
    redis = RedisManager(settings)
    mqtt = MqttService(settings)

    try:
        # MongoDB and Redis are required for API operations; fail fast if they are unavailable.
        await mongo.connect()
        await redis.connect()
        app.state.mongo = mongo
        app.state.redis = redis
        app.state.mqtt = mqtt
        app.state.device_repository = DeviceRepository(mongo.database)
        app.state.telemetry_repository = TelemetryRepository(mongo.database)
        app.state.clients_repository = ClientsRepository(mongo.database)
        app.state.loop = asyncio.get_running_loop()

        from app.services.telemetry_service import TelemetryService
        from app.services.device_service import DeviceService

        async def handle_telemetry(client_id, device_id, payload):
            telemetryService = TelemetryService(
                app.state.device_repository,
                app.state.telemetry_repository,
            )
            await telemetryService.ingest(client_id, device_id, payload)

        async def handle_device(client_id, device_id, payload
                                ):
            telemetryService = TelemetryService(
                app.state.device_repository,
                app.state.telemetry_repository,
            )
            doc = payload.model_dump()
            motor_state = doc.get("motor_state")

            await telemetryService._devices.upsert_device(device_id, state=motor_state)

        mqtt.set_telemetry_handler(handle_telemetry)
        mqtt.set_device_state_handler(handle_device)
        await mqtt.start()
        yield
    finally:
        await mqtt.stop()
        await redis.close()
        await mongo.close()


def create_app() -> FastAPI:
    settings = get_settings()
    application = FastAPI(
        title=settings.app_name,
        version="1.0.0",
        default_response_class=ORJSONResponse,
        lifespan=lifespan,
    )
    application.include_router(api_router, prefix=settings.api_prefix)
    application.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=False,
        allow_methods=["*"],
        allow_headers=["*"],
    )
    return application


app = create_app()
