from datetime import datetime
from pydantic import BaseModel, Field, ConfigDict


class DeviceCreate(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)
    device_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    # client_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    name: str = Field(min_length=1, max_length=120)
    location: str | None = Field(default=None, max_length=200)

class DeviceMapping(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)
    device_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    client_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")

class DeviceMode(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)
    device_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    mode: str

class DeviceResponse(BaseModel):
    client_id: str | None = None
    device_id: str
    mode: str = "AUTO"
    state: str = "OFF"
    name: str | None = None
    location: str | None = None
    created_at: datetime
    updated_at: datetime
