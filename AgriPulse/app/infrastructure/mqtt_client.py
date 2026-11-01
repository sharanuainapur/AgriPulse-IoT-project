import asyncio
import json
import logging
import threading
from typing import Callable

import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion

from app.core.config import Settings
from app.schemas.telemetry import TelemetryPayload
from app.utils.mqtt_topics import parse_device_topic

logger = logging.getLogger(__name__)


class MqttService:
    """Paho MQTT client; network callbacks are bridged safely to the asyncio loop."""

    def __init__(self, settings: Settings):
        self._settings = settings
        self._client = mqtt.Client(
            callback_api_version=CallbackAPIVersion.VERSION2,
            client_id=settings.mqtt_client_id,
            protocol=mqtt.MQTTv311,
            clean_session=True,
        )
        if settings.mqtt_username:
            self._client.username_pw_set(settings.mqtt_username,
                                         settings.mqtt_password.get_secret_value() if settings.mqtt_password else None)
        if settings.mqtt_tls:
            self._client.tls_set()
        self._connected = threading.Event()
        self._loop: asyncio.AbstractEventLoop | None = None
        self._telemetry_handler: Callable | None = None
        self._device_handler: Callable | None = None
        self._client.on_connect = self._on_connect
        self._client.on_disconnect = self._on_disconnect
        self._client.on_message = self._on_message

    @property
    def is_connected(self) -> bool:
        return self._connected.is_set()

    def set_telemetry_handler(self, handler: Callable) -> None:
        self._telemetry_handler = handler

    def set_device_state_handler(self, handler: Callable) -> None:
        self._device_handler = handler

    async def start(self) -> None:
        self._loop = asyncio.get_running_loop()
        # connect_async avoids blocking or raising during startup when the broker is temporarily unavailable.
        # Paho retries in the background after loop_start().
        self._client.reconnect_delay_set(min_delay=1, max_delay=30)
        self._client.connect_async(
            self._settings.mqtt_host,
            self._settings.mqtt_port,
            self._settings.mqtt_keepalive,
        )
        self._client.loop_start()
        try:
            await asyncio.wait_for(asyncio.to_thread(self._connected.wait), timeout=5)
        except asyncio.TimeoutError:
            logger.warning(
                "MQTT broker is not connected yet; API will start and MQTT reconnects in background"
            )

    async def stop(self) -> None:
        try:
            self._client.disconnect()
        except Exception:
            logger.debug("MQTT disconnect returned an error during shutdown", exc_info=True)
        try:
            self._client.loop_stop()
        except Exception:
            logger.exception("Error stopping MQTT network loop")

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            self._connected.set()
            logger.info("MQTT connected")
            # client.subscribe(f"{self._settings.mqtt_topic_prefix}/+/+/telemetry", qos=self._settings.mqtt_qos)
            # client.subscribe(f"{self._settings.mqtt_topic_prefix}/+/+/state", qos=self._settings.mqtt_qos)
            client.subscribe(f"{self._settings.mqtt_topic_prefix}/+/telemetry", qos=self._settings.mqtt_qos)
            client.subscribe(f"{self._settings.mqtt_topic_prefix}/+/state", qos=self._settings.mqtt_qos)
        else:
            logger.error("MQTT connection rejected: %s", reason_code)

    def _on_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        self._connected.clear()
        logger.warning("MQTT disconnected: %s", reason_code)

    def _on_message(self, client, userdata, message):
        if not self._loop or not self._telemetry_handler:
            return

        try:
            parsed = parse_device_topic(message.topic, self._settings.mqtt_topic_prefix)

            payload = json.loads(message.payload.decode("utf-8"))

            if parsed["kind"] == "state":
                payload = {
                    "motor_state": payload.get("motor_state"),
                }

            model = TelemetryPayload.model_validate(payload)

            asyncio.run_coroutine_threadsafe(
                self._telemetry_handler(None, parsed["device_id"], model),
                self._loop,
            )

            if parsed["kind"] == "state" and self._device_handler:
                asyncio.run_coroutine_threadsafe(
                    self._device_handler(None, parsed["device_id"], model),
                    self._loop,
                )

        except Exception:
            logger.exception(
                "Ignoring invalid MQTT message on topic %s",
                message.topic
            )

    async def subscribe_device(self, client_id: str, device_id: str) -> None:
        for kind in ("telemetry", "state"):
            # topic = f"{self._settings.mqtt_topic_prefix}/{client_id}/{device_id}/{kind}"
            topic = f"{self._settings.mqtt_topic_prefix}/{device_id}/{kind}"
            result, _ = self._client.subscribe(topic, qos=self._settings.mqtt_qos)
            if result != mqtt.MQTT_ERR_SUCCESS:
                raise RuntimeError(f"Could not subscribe to MQTT topic: {topic}")

    async def publish_json(self, topic: str, payload: dict, qos: int | None = None, retain: bool = False) -> None:
        if not self.is_connected:
            raise RuntimeError("MQTT broker is not connected")
        encoded = json.dumps(payload, separators=(",", ":"), ensure_ascii=False)
        info = self._client.publish(topic, encoded, qos=self._settings.mqtt_qos if qos is None else qos, retain=retain)
        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"MQTT publish failed with code {info.rc}")
        await asyncio.to_thread(info.wait_for_publish, 5)
        if not info.is_published():
            raise TimeoutError("MQTT publish timed out")

    async def publish(self, topic: str, payload: dict) -> None:
        if not self.is_connected:
            raise RuntimeError("MQTT broker is not connected")

        encoded = json.dumps(payload, separators=(",", ":"))

        info = self._client.publish(topic=topic, payload=encoded, qos=None, retain=False)

        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"MQTT publish failed with code: {info.rc}")

        await asyncio.to_thread(info.wait_for_publish, 5)

        if not info.is_published():
            raise TimeoutError("MQTT publish timed out")
