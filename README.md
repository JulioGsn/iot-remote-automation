# iot-remote-automation

Automação residencial com ESP32, Mosquitto MQTT e Home Assistant.

## Estrutura

- `src/main.cpp` — firmware ESP32 (5 automações: letreiro, home server, fechadura, temperatura, interfone)
- `diagram.json` — simulação Wokwi com labels visuais
- `docker-compose.yml` — Mosquitto + Home Assistant

## Como usar

```bash
docker compose up -d
# Acessar http://localhost:8123 e configurar integração MQTT
# broker: mosquitto, porta: 1883
```

Compilar firmware com PlatformIO e rodar no Wokwi ou gravar em um ESP32.
