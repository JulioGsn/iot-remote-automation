#!/bin/sh
set -eu

MQTT_HOST="${MQTT_HOST:-mosquitto}"
MQTT_PORT="${MQTT_PORT:-1883}"
INTERVAL="${INTERVAL:-5}"

publish() {
  mosquitto_pub -h "$MQTT_HOST" -p "$MQTT_PORT" -r -t "$1" -m "$2"
}

while true; do
  now="$(date +%s)"
  cpu=$((18 + now % 67))
  memory=$((38 + now % 44))
  temperature=$((36 + now % 28))
  processes=$((92 + now % 76))

  publish "home/server/status" "ONLINE"
  publish "home/server/telemetry/temperature" "$temperature"
  publish "home/server/telemetry/cpu" "$cpu"
  publish "home/server/telemetry/memory" "$memory"
  publish "home/server/telemetry/processes" "$processes"
  publish "home/server/telemetry/top_processes" "docker,home-assistant,mosquitto,wokwi-gateway,ssh"

  sleep "$INTERVAL"
done
