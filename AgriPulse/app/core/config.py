from functools import lru_cache
from pydantic import Field, SecretStr
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", case_sensitive=False, extra="ignore")

    app_name: str = "AgriPulse"
    app_env: str = "development"
    api_prefix: str = "/api/v1"
    host: str = "0.0.0.0"
    port: int = 8000
    log_level: str = "INFO"

    app_username: str = "admin"
    app_password: SecretStr = SecretStr("change-me-now")
    jwt_secret: SecretStr = SecretStr("replace-with-a-long-random-secret-at-least-32-characters")
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = Field(default=60, ge=1, le=1440)

    mongodb_uri: str = "mongodb://localhost:27017"
    mongodb_database: str = "agripulse"
    redis_url: str = "redis://localhost:6379/0"

    mqtt_host: str = "localhost"
    mqtt_port: int = Field(default=1883, ge=1, le=65535)
    mqtt_username: str | None = None
    mqtt_password: SecretStr | None = None
    mqtt_client_id: str = "agripulse-api"
    mqtt_keepalive: int = 60
    mqtt_qos: int = Field(default=1, ge=0, le=2)
    mqtt_tls: bool = False
    mqtt_topic_prefix: str = "agripulse"


@lru_cache
def get_settings() -> Settings:
    return Settings()
