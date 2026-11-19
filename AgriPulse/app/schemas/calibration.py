from datetime import datetime
from typing import Literal
from pydantic import BaseModel, Field, ConfigDict, model_validator

# Calibrations that can be triggered on the device over MQTT.
TriggerableCalibration = Literal["waterflow"]


class CalibrationCommandResponse(BaseModel):
    client_id: str
    device_id: str
    calibration: str
    topic: str
    message: str


class CalibrationData(BaseModel):
    dry_val: float
    wet_val: float

    @model_validator(mode="after")
    def check_distinct(self):
        if self.dry_val == self.wet_val:
            raise ValueError("dry_val and wet_val must be different")
        return self


class CalibrationUpsert(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)
    client_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    device_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    calibration: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    data: CalibrationData


class CalibrationResponse(BaseModel):
    client_id: str
    device_id: str
    calibration: str
    data: CalibrationData
    created_at: datetime
    updated_at: datetime
