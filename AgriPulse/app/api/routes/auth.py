from typing import Annotated
from fastapi import APIRouter, HTTPException, status
from app.core.config import get_settings
from app.core.security import create_access_token, verify_configured_password
from app.schemas.auth import LoginRequest, TokenResponse

router = APIRouter()


def _login(username: str, password: str, client_id: str) -> TokenResponse:
    settings = get_settings()
    if username != settings.app_username or not verify_configured_password(password):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Incorrect username or password")
    token, expires_in = create_access_token(username, client_id)
    return TokenResponse(access_token=token, expires_in=expires_in, client_id=client_id)


@router.post("/login", response_model=TokenResponse)
async def login(payload: LoginRequest):
    return _login(payload.username, payload.password, payload.client_id)

