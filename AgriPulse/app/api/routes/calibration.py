from fastapi import APIRouter, Depends, HTTPException, Response, status
from app.core.dependencies import get_calibration_repository, get_mqtt
from app.schemas.calibration import (
    CalibrationUpsert,
    CalibrationResponse,
    CalibrationCommandResponse,
    TriggerableCalibration,
)
from app.services.calibration_service import CalibrationService

router = APIRouter()


@router.post(
    "/{client_id}/{device_id}/{calibration}",
    response_model=CalibrationCommandResponse,
    status_code=status.HTTP_202_ACCEPTED,
)
async def trigger_calibration(
        client_id: str,
        device_id: str,
        calibration: TriggerableCalibration,
        repository=Depends(get_calibration_repository),
        mqtt=Depends(get_mqtt),
):
    service = CalibrationService(repository, mqtt)
    try:
        topic = await service.publish_calibration_command(client_id, device_id, calibration)
    except TimeoutError as exc:
        raise HTTPException(status_code=504, detail=str(exc)) from exc
    except RuntimeError as exc:
        raise HTTPException(status_code=503, detail=str(exc)) from exc

    return CalibrationCommandResponse(
        client_id=client_id,
        device_id=device_id,
        calibration=calibration,
        topic=topic,
        message="Calibration command published; await device response",
    )


@router.put("", response_model=CalibrationResponse)
async def upsert_calibration(
        payload: CalibrationUpsert,
        repository=Depends(get_calibration_repository),
):
    service = CalibrationService(repository)
    try:
        return await service.save(payload)
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc


@router.get("/{client_id}/{device_id}", response_model=list[CalibrationResponse])
async def list_calibrations(
        client_id: str,
        device_id: str,
        repository=Depends(get_calibration_repository),
):
    return await CalibrationService(repository).list_by_device(client_id, device_id)


@router.get("/{client_id}/{device_id}/{calibration}", response_model=CalibrationResponse)
async def get_calibration(
        client_id: str,
        device_id: str,
        calibration: str,
        repository=Depends(get_calibration_repository),
):
    try:
        return await CalibrationService(repository).get(client_id, device_id, calibration)
    except LookupError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.delete("/{client_id}/{device_id}/{calibration}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_calibration(
        client_id: str,
        device_id: str,
        calibration: str,
        repository=Depends(get_calibration_repository),
):
    try:
        await CalibrationService(repository).delete(client_id, device_id, calibration)
    except LookupError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return Response(status_code=status.HTTP_204_NO_CONTENT)
