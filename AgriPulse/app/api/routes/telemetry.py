from fastapi import APIRouter, Depends, HTTPException
from app.core.dependencies import get_current_user, get_device_repository, get_telemetry_repository
from app.schemas.telemetry import LatestTelemetryResponse
from app.services.telemetry_service import TelemetryService

router = APIRouter()


@router.get("/{device_id}/sensors/latest", response_model=LatestTelemetryResponse)
async def latest_sensors(
    device_id: str,
    user: dict = Depends(get_current_user),
    devices=Depends(get_device_repository),
    telemetry=Depends(get_telemetry_repository),
):
    service = TelemetryService(devices, telemetry)
    result = await service.latest(user["client_id"], device_id)
    if result is None:
        raise HTTPException(status_code=404, detail="Device or telemetry not found")
    return result
