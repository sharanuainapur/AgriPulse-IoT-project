from datetime import datetime, timedelta, timezone
import hmac
import jwt
from fastapi import HTTPException, status

from app.core.config import get_settings



def verify_configured_password(candidate: str) -> bool:
    # Constant-time comparison for the configured bootstrap password.
    configured = get_settings().app_password.get_secret_value()
    return hmac.compare_digest(candidate.encode("utf-8"), configured.encode("utf-8"))


def create_access_token(username: str, client_id: str) -> tuple[str, int]:
    settings = get_settings()
    expires_in = settings.jwt_expire_minutes * 60
    now = datetime.now(timezone.utc)
    payload = {
        "sub": username,
        "client_id": client_id,
        "iat": now,
        "exp": now + timedelta(seconds=expires_in),
    }
    token = jwt.encode(payload, settings.jwt_secret.get_secret_value(), algorithm=settings.jwt_algorithm)
    return token, expires_in


def decode_access_token(token: str) -> dict:
    settings = get_settings()
    try:
        payload = jwt.decode(token, settings.jwt_secret.get_secret_value(), algorithms=[settings.jwt_algorithm])
        if not payload.get("sub") or not payload.get("client_id"):
            raise ValueError("Required claims missing")
        return payload
    except (jwt.PyJWTError, ValueError) as exc:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid or expired access token") from exc
