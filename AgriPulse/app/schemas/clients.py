from datetime import datetime
from pydantic import BaseModel, Field, ConfigDict


class ClientsCreate(BaseModel):
    model_config = ConfigDict(str_strip_whitespace=True)
    client_id: str = Field(min_length=1, max_length=100, pattern=r"^[a-zA-Z0-9_-]+$")
    name: str = Field(min_length=1, max_length=120)
    phone: str = Field(min_length=1, max_length=120)


class ClientResponse(BaseModel):
    client_id: str
    name: str
    phone: str
    created_at: datetime
    updated_at: datetime
