from fastapi import APIRouter
from app.api.routes import auth, devices, health, telemetry, motor, clients

api_router = APIRouter()
api_router.include_router(health.router, prefix="/health", tags=["health"])
api_router.include_router(auth.router, prefix="/auth", tags=["authentication"])
api_router.include_router(devices.router, prefix="/devices", tags=["devices"])
api_router.include_router(telemetry.router, prefix="/devices", tags=["telemetry"])
api_router.include_router(motor.router, prefix="/devices", tags=["motor"])
api_router.include_router(clients.router, prefix="/clients", tags=["clients"])
