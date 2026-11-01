import pytest
from pydantic import ValidationError
from app.schemas.telemetry import TelemetryPayload
from app.schemas.auth import LoginRequest


def test_telemetry_bounds():
    with pytest.raises(ValidationError):
        TelemetryPayload(soil_moisture_pct=101)


def test_login_client_id_validation():
    with pytest.raises(ValidationError):
        LoginRequest(username="u", password="p", client_id="bad client id")
