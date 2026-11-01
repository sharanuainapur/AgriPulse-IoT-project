from typing import Annotated
from fastapi import Depends, Request
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

from app.core.security import decode_access_token

bearer_scheme = HTTPBearer(auto_error=True)


def get_current_user(
    credentials: Annotated[HTTPAuthorizationCredentials, Depends(bearer_scheme)],
) -> dict:
    return decode_access_token(credentials.credentials)


def get_mongo(request: Request):
    return request.app.state.mongo


def get_redis(request: Request):
    return request.app.state.redis


def get_mqtt(request: Request):
    return request.app.state.mqtt


def get_device_repository(request: Request):
    return request.app.state.device_repository

def get_clients_repository(request: Request):
    return request.app.state.clients_repository

def get_telemetry_repository(request: Request):
    return request.app.state.telemetry_repository
