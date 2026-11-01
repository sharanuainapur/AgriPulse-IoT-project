from datetime import datetime, timezone
from typing import Literal
from pydantic import BaseModel, ConfigDict, Field, field_validator


class TelemetryPayload(BaseModel):
    model_config = ConfigDict(extra="ignore")
    soil_moisture_pct: float | None = Field(default=None, ge=0, le=100)
    temperature_c: float | None = Field(default=None, ge=-80, le=150)
    water_flow_lpm: float | None = Field(default=None, ge=0, le=100000)
    motor_state: Literal["ON", "OFF"] | None = None
    recorded_at: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))

    @field_validator("recorded_at")
    @classmethod
    def ensure_timezone(cls, value: datetime) -> datetime:
        if value.tzinfo is None:
            return value.replace(tzinfo=timezone.utc)
        return value


class LatestTelemetryResponse(BaseModel):
    client_id: str
    device_id: str
    soil_moisture_pct: float | None = None
    temperature_c: float | None = None
    water_flow_lpm: float | None = None
    motor_state: Literal["ON", "OFF"] | None = None
    recorded_at: datetime
    received_at: datetime


class MotorStateResponse(BaseModel):
    device_id: str
    motor_state: Literal["ON", "OFF"] | None = None
    reported_at: datetime | None = None
