from fastapi import APIRouter, Depends, HTTPException, status
from app.core.dependencies import get_current_user, get_clients_repository, get_mqtt
from app.schemas.clients import ClientsCreate, ClientResponse
from app.services.clients_service import ClientsService

router = APIRouter()


@router.post("", response_model=ClientResponse, status_code=status.HTTP_201_CREATED)
async def register_clients(
        payload: ClientsCreate,
        repository=Depends(get_clients_repository),
        mqtt=Depends(get_mqtt),
):
    service = ClientsService(repository, mqtt)
    try:
        return await service.register(payload)
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc

#
@router.get("", response_model=list[ClientResponse])
async def list_devices(
    repository=Depends(get_clients_repository),
):
    service = ClientsService(repository, None)
    try:
        return await service.list()
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
