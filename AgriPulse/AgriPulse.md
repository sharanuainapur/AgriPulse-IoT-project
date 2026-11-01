# AgriPulse — AI Project Guide

## Purpose
AgriPulse is a reusable FastAPI backend for IoT irrigation monitoring. It registers devices under client accounts, consumes MQTT telemetry, persists readings in MongoDB, checks Redis/MongoDB/MQTT health, and publishes motor commands.

## Architecture

```text
HTTP client
   |
   v
FastAPI routers --> Auth/JWT --> DeviceService / TelemetryService
   |                                  |           |
   |                                  v           v
   |                              MongoDB       Redis
   |
   v
MqttService <---- MQTT broker <---- IoT devices
   |                  |
   +---- subscribe ---+---- telemetry/state topics
   +---- publish ---------- command topics
```

### Module responsibilities
- `app/main.py`: application factory and lifespan orchestration.
- `app/core/config.py`: typed environment configuration.
- `app/core/security.py`: password comparison and JWT issue/validation.
- `app/api/`: HTTP route definitions and dependency wiring.
- `app/schemas/`: request/response validation models.
- `app/services/`: business logic, ownership checks, telemetry reads, command publication.
- `app/infrastructure/`: MongoDB, Redis, and MQTT clients.
- `app/repositories/`: database access, isolated from HTTP concerns.
- `app/utils/`: topic parsing and common helpers.

## Coding conventions for AI contributors
1. Keep route handlers thin; business rules belong in services.
2. Keep database queries in repositories.
3. Use Pydantic models at external boundaries.
4. Add type hints and meaningful names.
5. Never trust `client_id` from a request body for authorization. Use the JWT claim.
6. Always scope device access by both `client_id` and `device_id`.
7. Never log passwords, JWTs, or full sensitive payloads.
8. Keep MQTT callbacks fast; schedule async persistence safely onto the app event loop.
9. Validate MQTT topic structure and payloads before persisting.
10. Add tests for each new behavior and failure path.
11. Keep secrets in environment variables or a secret manager.
12. A publish acknowledgment is not confirmation that a physical actuator changed state.

## MQTT contract
- `agripulse/{client_id}/{device_id}/telemetry`: JSON sensor telemetry.
- `agripulse/{client_id}/{device_id}/state`: JSON device state, e.g. `{"motor_state":"ON"}`.
- `agripulse/{client_id}/{device_id}/command`: JSON command published by the backend.

## Data model
- `devices`: `client_id`, `device_id`, `name`, `location`, `created_at`, `updated_at`.
- `telemetry`: `client_id`, `device_id`, sensor readings, `motor_state`, `recorded_at`, `received_at`.
- Sensor readings are append-only; API selects the latest document.

## Important engineering notes
- The configured login is a single-user bootstrap mechanism, not a full multi-tenant identity system.
- JWT `client_id` is an authorization boundary.
- In a horizontally scaled deployment, use shared broker subscriptions carefully and consider MQTT shared subscriptions or a dedicated ingestion worker.
- MQTT callbacks use Paho's network thread; persistence is handed to the asyncio event loop.
- MongoDB/Redis readiness is checked independently from liveness.
