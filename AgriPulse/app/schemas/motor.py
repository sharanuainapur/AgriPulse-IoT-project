from datetime import datetime
from typing import Literal
from pydantic import BaseModel


class MotorCommandRequest(BaseModel):
    state: str


class MotorCommandResponse(BaseModel):
    device_id: str
    state: str


class MotorStateResponse(BaseModel):
    device_id: str
    state: str
    reported_at: datetime | None = None
