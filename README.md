# iot-remote-automation

Automação residencial com ESP32, Mosquitto MQTT e Home Assistant.

## Estrutura

- `src/main.cpp` — firmware ESP32 (5 automações: letreiro, home server, fechadura, temperatura, interfone)
- `diagram.json` — simulação Wokwi com labels visuais
- `docker-compose.yml` — Mosquitto + Home Assistant

## Como usar

```bash
docker compose up -d
```

Acesse o Home Assistant em http://localhost:8124. A integração MQTT já fica
pré-configurada para o broker Docker `mosquitto:1883`.

Se a porta `8123` estiver livre e você preferir usar a porta padrão do Home
Assistant:

```bash
HA_PORT=8123 docker compose up -d
```

O broker MQTT fica exposto em `localhost:1883`. Para o Wokwi acessar esse broker
local, rode/habilite o **Wokwi Private IoT Gateway** e inicie a simulação pelo
VS Code. No firmware, o `MQTT_SERVER` está como `host.wokwi.internal`, que é o
hostname do computador host visto pela simulação.

No Wokwi, abra o Command Palette (`F1`) e use **Enable Private Wokwi IoT
Gateway**. Sem esse gateway, o ESP32 simulado não consegue acessar serviços
rodando em `localhost`/Docker na sua máquina.

## Validação rápida

```bash
docker compose ps
docker compose exec mosquitto mosquitto_pub -h localhost -r -t test -m ping
docker compose exec mosquitto mosquitto_sub -h localhost -t test -C 1
```

Compilar firmware:

```bash
/home/juliogsn/.platformio/penv/bin/pio run
```

Depois rode no Wokwi ou grave em um ESP32.
