from fastapi import APIRouter, Depends, HTTPException
from app.core.dependencies import get_current_user, get_device_repository, get_telemetry_repository, get_mqtt
from app.schemas.motor import MotorCommandRequest, MotorCommandResponse, MotorStateResponse
from app.services.device_service import DeviceService
from app.schemas.device import DeviceMode
from app.services.telemetry_service import TelemetryService

import logging

logger = logging.getLogger(__name__)

router = APIRouter()



@router.get("/{device_id}/motor", response_model=MotorStateResponse)
async def motor_state(
        device_id: str,
        user: dict = Depends(get_current_user),
        devices=Depends(get_device_repository),
        telemetry=Depends(get_telemetry_repository),
):
    if not await devices.get_by_client_and_device(user["client_id"], device_id):
        raise HTTPException(status_code=404, detail="Device not found")

    result = await devices.get_by_device(device_id)

    if not result:
        raise HTTPException(status_code=404, detail="Motor state not found")

    return MotorStateResponse(
        device_id=device_id,
        state=result.get("state") or "OFF",
    )


@router.put("/{device_id}/motor", response_model=MotorCommandResponse)
async def control_motor(
        device_id: str,
        payload: MotorCommandRequest,
        user: dict = Depends(get_current_user),
        devices=Depends(get_device_repository),
        mqtt=Depends(get_mqtt),
):
    device = await devices.get_by_client_and_device(user["client_id"], device_id)

    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    device_service = DeviceService(devices, mqtt)

    d = await devices.upsert_device(device_id, mode="MANUAL", state=payload.state)

    # publish to MQTT
    topic = await device_service.publish_motor_command(device_id, payload.state)

    return MotorCommandResponse(
        device_id=device_id,
        state=payload.state,
        topic=topic,
        message="Command published; await device state/telemetry for confirmation",
    )
