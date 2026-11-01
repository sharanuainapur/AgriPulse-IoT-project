# AgriPulse

Production-minded FastAPI starter for IoT agriculture monitoring and motor control. The code is split by responsibility, uses configuration from environment variables, validates API inputs, scopes devices to a `client_id`, stores telemetry in MongoDB, uses Redis for a small health/cache capability, and integrates MQTT for telemetry subscriptions and command publishing.

> **Security note:** This is a strong starter architecture, not a substitute for a deployment security review. Configure strong secrets, TLS, broker authentication, network restrictions, backups, monitoring, and rate limiting before exposing it publicly. The sample login uses one configured application user; replace it with your identity provider or a persisted user model for multi-user production.

## Features

- FastAPI lifespan-managed startup/shutdown
- Liveness/readiness health endpoints
- Username/password login returning a JWT bearer token
- Register IoT devices under a `client_id`
- Latest soil, temperature, and water-flow telemetry per device
- Motor-state endpoint and motor control endpoint
- MQTT subscribe for telemetry and publish commands
- MongoDB persistence and Redis connectivity check
- Reusable MQTT publishing service
- Docker Compose for the API, MongoDB, Redis, and Mosquitto
- Structured module layout, typed settings, validation, and tests

## Quick start

1. Install Docker and Docker Compose.
2. Copy environment defaults:

   ```bash
   cp .env.example .env
   ```
3. Change `JWT_SECRET`, `APP_USERNAME`, and `APP_PASSWORD` in `.env`.
4. Start the stack:

   ```bash
   docker compose up --build
   ```
5. Open `http://localhost:8000/docs`.

Default development credentials are `admin` / `change-me-now`. Change them before use. MongoDB and Redis must be healthy at startup. MQTT may connect after the API starts; check `/api/v1/health/ready`.

## API overview

All endpoints except `/api/v1/health/live`, `/api/v1/health/ready`, and `/api/v1/auth/login` require `Authorization: Bearer <token>`.

- `GET /api/v1/health/live` — process liveness
- `GET /api/v1/health/ready` — MongoDB, Redis, and MQTT readiness
- `POST /api/v1/auth/login` — get JWT
- `POST /api/v1/devices` — register a device
- `GET /api/v1/devices` — list devices belonging to the authenticated `client_id`
- `GET /api/v1/devices/{device_id}/sensors/latest` — latest sensor values
- `GET /api/v1/devices/{device_id}/motor` — last reported motor state
- `PUT /api/v1/devices/{device_id}/motor` — publish motor command

### Login

```bash
curl -X POST http://localhost:8000/api/v1/auth/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"admin","password":"change-me-now","client_id":"farm-001"}'
```

Copy `access_token` from the response and set it as `TOKEN`.

### Register device

```bash
curl -X POST http://localhost:8000/api/v1/devices \
  -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"device_id":"pump-001","name":"North field pump","location":"North field"}'
```

### MQTT topic contract

For client `farm-001`, device `pump-001`:

- Telemetry subscribe: `agripulse/farm-001/pump-001/telemetry`
- Motor-state subscribe: `agripulse/farm-001/pump-001/state`
- Motor command publish: `agripulse/farm-001/pump-001/command`

Telemetry JSON example:

```json
{
  "soil_moisture_pct": 42.5,
  "temperature_c": 27.1,
  "water_flow_lpm": 3.2,
  "motor_state": "ON",
  "recorded_at": "2026-10-15T00:00:00Z"
}
```

The API subscribes to `agripulse/+/+/telemetry` and `agripulse/+/+/state`. Device registration also subscribes to the device-specific topics. Ensure your broker ACLs allow only the intended clients/topics.

Motor command JSON example:

```json
{"command":"SET_MOTOR","state":"ON","request_id":"..."}
```

A successful publish means the broker accepted the publish operation; it does **not** prove the physical motor changed state. The device should publish a later state/telemetry message to confirm.

## Data and ownership

JWT includes a `client_id`. Device endpoints always scope lookups by both `device_id` and authenticated `client_id`. The device ID must be unique within a client. Sensor records are stored as append-only telemetry documents; latest data is selected by `recorded_at` and `_id`.

## Configuration

See `.env.example`. Never commit `.env` or production credentials. In production, use a secret manager, TLS for Redis/MongoDB/MQTT, broker ACLs, and a proper identity service.

## Tests

```bash
python3 -m venv .venv
source .venv/bin/activate
pip3 install -r requirements.txt
pytest
```

Tests cover schema validation and topic parsing without requiring infrastructure.

## Suggested production hardening

- Use HTTPS behind a reverse proxy/load balancer.
- Add login throttling, audit logs, and key rotation.
- Use unique per-user credentials and a proper password-hashing flow if users are stored in a database.
- Configure MongoDB indexes and retention policy according to data volume.
- Add MQTT TLS, username/password or certificates, ACLs, QoS review, and message-size limits.
- Define sensor freshness thresholds and alerting.
- Add metrics, tracing, log aggregation, backups, and restore tests.
- Add idempotency for device registration and command requests.
- Consider a durable command/outbox collection if command delivery must survive process restarts.


## If startup fails

Run `docker compose logs --tail=200 api mongodb redis mosquitto` to identify the first traceback. The API intentionally fails startup if MongoDB or Redis cannot connect; MQTT reconnects in the background. If you changed `.env`, recreate containers with `docker compose up --build -d`.
