from fastapi import APIRouter, Depends, HTTPException, status
from app.core.dependencies import get_current_user, get_device_repository, get_mqtt
from app.schemas.device import DeviceCreate, DeviceResponse, DeviceMapping, DeviceMode
from app.services.device_service import DeviceService

router = APIRouter()


@router.post("", response_model=DeviceResponse, status_code=status.HTTP_201_CREATED)
async def register_device(payload: DeviceCreate, user: dict = Depends(get_current_user),
                          repository=Depends(get_device_repository), mqtt=Depends(get_mqtt)):
    service = DeviceService(repository, mqtt)
    try:
        return await service.register(payload)
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc

@router.post("/mapping", response_model=DeviceResponse, status_code=status.HTTP_201_CREATED)
async def register_device(payload: DeviceMapping, user: dict = Depends(get_current_user),
                          repository=Depends(get_device_repository), mqtt=Depends(get_mqtt)):
    service = DeviceService(repository, mqtt)
    try:
        return await service.mapping(payload)
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc

@router.get("", response_model=list[DeviceResponse])
async def list_devices(
        user: dict = Depends(get_current_user),
        repository=Depends(get_device_repository),
):
    return await repository.list_by_client(user["client_id"])

@router.put("/mode", response_model=DeviceResponse)
async def set_mode(
    payload: DeviceMode,
    user: dict = Depends(get_current_user),
    repository=Depends(get_device_repository),
    mqtt=Depends(get_mqtt),
):
    service = DeviceService(repository, mqtt)
    try:
        return await service.update_mode(payload, True)
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
