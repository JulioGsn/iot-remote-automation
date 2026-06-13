# Checklist — Home Office IoT

---

## 1. Infraestrutura Docker

- [ ] **Mosquitto rodando**
      `docker compose ps` → `iot-mosquitto` deve estar `Up`
      Teste: `docker compose exec mosquitto mosquitto_sub -h localhost -t "test" -C 1`
      Em outro terminal: `docker compose exec mosquitto mosquitto_pub -h localhost -t "test" -m "ping"`
      Se receber "ping" no subscriber, o broker está OK.

- [ ] **Home Assistant rodando**
      `docker compose ps` → `iot-homeassistant` deve estar `Up`
      Acessar http://localhost:8124 — deve carregar o onboarding ou o dashboard.
      Se quiser usar a porta padrão e ela estiver livre: `HA_PORT=8123 docker compose up -d`.

- [ ] **Telemetria do Home Server rodando**
      `docker compose ps` → `iot-home-server-telemetry` deve estar `Up`.
      Teste:
      ```bash
      docker compose exec mosquitto mosquitto_sub \
        -h localhost -t "home/server/#" -C 6 -v
      ```
      Deve mostrar `home/server/status ONLINE`, CPU, RAM, temperatura e processos.

- [ ] **Home Assistant: integração MQTT configurada**
      Settings → Devices & Services → deve aparecer "MQTT" como integração.
      A entrada já vem pré-configurada em `homeassistant/config/.storage/core.config_entries`.
      Broker: `mosquitto` / Porta: `1883` / Sem usuário/senha.

---

## 2. Firmware ESP32

- [ ] **Código compila sem erros**
      `pio run` (ou via PlatformIO no VS Code) → deve finalizar com `SUCCESS`.

- [ ] **Configurações de rede corretas** (`src/main.cpp`)
      - `WIFI_SSID` — qualquer string (Wokwi simula, não valida)
      - `MQTT_SERVER` — `"host.wokwi.internal"` para Wokwi com Private IoT Gateway, ou IP local da máquina para HW real
      - `MQTT_USER` / `MQTT_PASSWORD` — vazio (acesso anônimo)

- [ ] **Firmware compilada atualizada**
      Após qualquer mudança no código, rodar `pio run` novamente.
      O Wokwi carrega o `.pio/build/esp32dev/firmware.elf`.

---

## 3. Simulação Wokwi (VS Code)

- [ ] **Extensão Wokwi instalada**
      VS Code → Extensions → "Wokwi for VS Code"

- [ ] **Simulação inicia sem erros**
      Abrir `diagram.json` → clicar "Start Simulation" (ícone verde no canto superior direito).
      Deve mostrar o diagrama com ESP32 + componentes + labels visíveis.

- [ ] **Private Wokwi IoT Gateway habilitado**
      No VS Code, abrir Command Palette (`F1`) → **Enable Private Wokwi IoT Gateway**.
      Necessário para o ESP32 simulado acessar o Mosquitto local em `localhost:1883`.
      Sem isso, o Serial Monitor fica com `[MQTT] Falha (rc=-2/-4)` e o Mosquitto não mostra o cliente `esp32_home_office`.

- [ ] **Serial Monitor aparecendo**
      No Wokwi panel ou View → Serial Monitor.
      Deve mostrar as mensagens `Serial.print()` do ESP32 com baud 115200.

- [ ] **WiFi conecta**
      Serial Monitor deve mostrar:
      ```
      [WiFi] Conectando a Wokwi-GUEST ...
      [WiFi] Conectado! IP: 10.0.2.x
      ```
      Se não aparecer, verificar:
      - O WiFi está habilitado no código (`WiFi.mode(WIFI_STA)`)
      - Tempo de timeout: o código tenta a cada 10s (`WIFI_RETRY_MS`)

- [ ] **MQTT conecta**
      Serial Monitor deve mostrar:
      ```
      [MQTT] Conectado!
      ```
      Se aparecer `[MQTT] Falha (rc=X)`:
      - rc=5 → conexão recusada. Mosquitto está rodando? O `MQTT_SERVER` está correto?
      - rc=4 → erro de rede. WiFi realmente conectou?
      - rc=3 → broker inacessível no endereço/porta.

---

## 4. Comunicação MQTT (teste manual)

Testar cada tópico que o ESP32 escuta. Publicar do terminal e verificar:

- [ ] **Letreiro Reunião**
      ```bash
      docker compose exec mosquitto mosquitto_pub \
        -h localhost -t "home/escritorio/status/cmd" -m "reuniao"
      ```
      NeoPixel deve acender VERMELHO no simulador.

      ```bash
      docker compose exec mosquitto mosquitto_pub \
        -h localhost -t "home/escritorio/status/cmd" -m "livre"
      ```
      NeoPixel deve acender VERDE.

- [ ] **Pulso Power Home Server**
      ```bash
      docker compose exec mosquitto mosquitto_pub \
        -h localhost -t "home/server/power/cmd" -m "LIGAR"
      ```
      Relé 1 (LED vermelho "Home Server") deve acender por ~1s.
      Estado MQTT: `home/server/power/state` deve ir para "ON" e voltar para "OFF".

- [ ] **Fechadura**
      ```bash
      docker compose exec mosquitto mosquitto_pub \
        -h localhost -t "home/seguranca/porta/cmd" -m "DESTRANCAR"
      ```
      Servo deve girar para 90°.
      Deve publicar estado: `home/seguranca/porta/state` → "DESTRANCADO"

- [ ] **Portão Interfone**
      ```bash
      docker compose exec mosquitto mosquitto_pub \
        -h localhost -t "home/interfone/abrir" -m "ABRIR"
      ```
      Relé 2 (LED azul "Portão") deve acender por ~3s.

- [ ] **Sensor publica temperatura**
      Aguardar ~5s (ou verificar Serial Monitor):
      ```
      [DHT22] Temperatura: XX.X C
      ```
      Tópico `home/cozinha/fogao/temperatura` deve receber o valor.
      Testar:
      ```bash
      docker compose exec mosquitto mosquitto_sub \
        -h localhost -t "home/cozinha/fogao/temperatura" -C 1
      ```

- [ ] **Botão Interfone (evento físico)**
      Clicar no botão no simulador Wokwi.
      Serial Monitor deve mostrar `[Interfone] TOCANDO!`
      Tópico `home/interfone/status` deve receber "TOCANDO".
      O estado fica "TOCANDO" por ~8s, ou volta para "SILENCIO" imediatamente quando o portão é aberto.

---

## 5. Home Assistant — Entidades

Após subir o Home Assistant com a integração MQTT pré-configurada, verificar:

- [ ] **select.letreiro_reuniao** aparece em Settings → Devices & Services → MQTT
      Mudar o valor para "reuniao" → NeoPixel deve ficar vermelho no simulador.

- [ ] **button.pulso_power_home_server** aparece
      Pressionar → Relé 1 deve ativar por 1s.

- [ ] **Sensores do Home Server aparecem**
      - `binary_sensor.home_server_online`
      - `sensor.home_server_temperatura`
      - `sensor.home_server_cpu`
      - `sensor.home_server_ram`
      - `sensor.home_server_processos`
      - `sensor.home_server_processos_principais`

- [ ] **lock.fechadura_porta** aparece
      Destrancar → Servo deve girar 90°.
      Estado deve refletir "Trancado"/"Destrancado".

- [ ] **button.abrir_portao_interfone** aparece
      Pressionar → Relé 2 deve ativar por 3s.
      Se o interfone estiver tocando, o status deve voltar para "SILENCIO".

- [ ] **sensor.temperatura_fogao** aparece
      Deve mostrar valor numérico com "°C".

- [ ] **binary_sensor.alerta_temperatura** aparece
      Estado "off" normalmente. Se temperatura > 60°C, vai para "on".

- [ ] **binary_sensor.interfone_tocando** aparece
      Estado "off". Quando botão pressionado, vai para "on".

Se as entidades aparecerem como **"unknown"** ou **"unavailable"**:
- A integração MQTT não está conectada ao broker.
- Verificar passo 1 (broker rodando?) e passo 3 (HA → MQTT pré-configurado).

---

## 6. Dashboard Lovelace

- [ ] **Abas visíveis**: Escritório, Home Server, Segurança, Cozinha.
- [ ] **Card Letreiro Reunião** — select com opções livre/foco/reuniao.
- [ ] **Card Pulso Power Home Server** — botão de pressão.
- [ ] **Aba Home Server** — status online, botão power, gauges de temperatura/CPU/RAM e processos.
- [ ] **Card Fechadura** — lock com comando travar/destrancar.
- [ ] **Card Portão Interfone** — botão de pressão.
- [ ] **Alerta Interfone Tocando** — aparece na aba Segurança quando `binary_sensor.interfone_tocando` está ON.
- [ ] **Card Temperatura** — sensor numérico.
- [ ] **Card Alerta Temperatura** — binary sensor (on/off).
- [ ] **Card Interfone Status** — binary sensor.

Se o dashboard não carregar:
- `configuration.yaml` tem `lovelace: mode: yaml`?
- `ui-lovelace.yaml` existe em `homeassistant/config/`?
- As entity_ids no YAML correspondem aos nomes gerados pelo HA? (Ex: `sensor.temperatura_fogao`)

---

## 7. Fluxo ponta-a-ponta (teste completo)

1. Docker rodando (Mosquitto + HA)
2. Simulação Wokwi rodando com firmware compilado
3. Serial Monitor mostra WiFi + MQTT conectados
4. No Home Assistant, mudar **Letreiro Reunião** para "reuniao"
5. **Resultado esperado**: NeoPixel no simulador acende VERMELHO
6. No HA, mudar para "livre" → NeoPixel acende VERDE
7. No HA, clicar em **Abrir Portão** → Relé 2 azul acende 3s
8. Pressionar o **botão físicio** no simulador → HA mostra "Interfone Tocando: ON"

---

## Problemas comuns

| Problema | Causa provável | Solução |
|---|---|---|
| WiFi não conecta no Wokwi | SSID vazio ou muito curto | Usar `"Wokwi-GUEST"` |
| MQTT rc=5 | Broker não acessível | Verificar se Mosquitto está rodando e `MQTT_SERVER` correto |
| MQTT rc=3 | Timeout de rede | WiFi não conectou de fato |
| Entidades HA como "unknown" | ESP32 ainda não publicou estado | Iniciar a simulação Wokwi ou publicar estados retidos de teste |
| Entidades HA "unavailable" | Broker caiu após configurado | Verificar `docker compose ps` |
| Dashboard não aparece | `lovelace: mode: yaml` ausente | Adicionar ao `configuration.yaml` e reiniciar HA |
| Comando não afeta o simulador | Tópico MQTT não corresponde | Verificar `#define` no `main.cpp` vs `configuration.yaml` |
| Serial Monitor vazio | Baud rate errado ou sem `serialMonitor` no diagram.json | Verificar `diagram.json` → `serialMonitor.baud: 115200` |
