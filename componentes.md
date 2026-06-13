# Componentes do Projeto — Home Office IoT

## Visão Geral

Este projeto utiliza um **ESP32** como controlador central para automatizar um home office, integrando sensores e atuadores via **WiFi** e **MQTT**. Abaixo, a justificativa e o funcionamento de cada componente.

---

## 1. ESP32 (Controlador Central)

| Especificação | Detalhe |
|---|---|
| **Modelo** | ESP32 DevKit V1 (WROOM-32) |
| **Núcleo** | Dual-core Xtensa LX6 @ 240 MHz |
| **WiFi / BLE** | Integrados |
| **GPIO disponíveis** | ~25 |

### Por que foi escolhido
- **Custo-benefício**: extremamente barato (~R\$ 40-60) frente a soluções como Raspberry Pi.
- **WiFi nativo**: elimina a necessidade de módulo externo (ex: ESP8266 ou shield Ethernet).
- **Suporte Arduino**: vasto ecossistema de bibliotecas (NeoPixel, DHT, Servo, MQTT).
- **Consumo energético**: roda em 3,3V com consumo médio de ~80 mA.

### Como funciona no projeto
- Executa o loop principal (`loop()`) que coordena todas as ações: conexão WiFi/MQTT, leitura de sensores, acionamento de atuadores e processamento de comandos via MQTT.
- Usa lógica **não-bloqueante** (`millis()`) para garantir que múltiplas tarefas ocorram simultaneamente sem travar.

---

## 2. LED Onboard (GPIO2)

### Como funciona no projeto
Indicador visual de status de conexão:
- **Aceso fixo**: WiFi e MQTT conectados.
- **Piscando (500 ms)**: sem conexão com o broker MQTT.

Elimina a necessidade de debug serial para saber o estado da conectividade.

---

## 3. Fita NeoPixel — WS2812B (GPIO19)

| Especificação | Detalhe |
|---|---|
| **Tipo** | LED RGB endereçável |
| **Quantidade** | 8 LEDs |
| **Protocolo** | Single-wire (DIN) |
| **Tensão** | 5V (alimentado via pino 3V3 do ESP32 para poucos LEDs) |

### Por que foi escolhida
- **Comunicação serial com 1 fio só**: ocupa apenas 1 GPIO.
- **Efeitos visuais**: cada LED é endereçável individualmente, permitindo cores e animações.
- **Biblioteca consolidada**: `Adafruit_NeoPixel` é madura e simples de usar.

### Como funciona no projeto
Funciona como um **letreiro de reunião** — ao receber um comando MQTT no tópico `home/escritorio/status/cmd`, acende:
- **Vermelho** → `"reuniao"` (não perturbe)
- **Amarelo** → `"foco"` (modo foco, não interrompa)
- **Verde** → `"livre"` (pode falar comigo)

---

## 4. Relé 1 — Home Server (GPIO18)

| Especificação | Detalhe |
|---|---|
| **Tipo** | Relé eletromecânico (NA/NF) |
| **Tensão da bobina** | 5V |
| **Corrente máxima** | ~10A |

### Por que foi escolhido
- **Isolamento galvânico**: protege o ESP32 do circuito de alta corrente do servidor.
- **Simplicidade**: acionamento por sinal digital HIGH/LOW.

### Como funciona no projeto
Simula o botão **Power** do Home Server. Ao receber `"LIGAR"` no tópico `home/server/power/cmd`, o ESP32 ativa o relé por **1 segundo** (pulso) e depois desliga. Isso equivale a pressionar o botão físico de power — útil para religar o servidor remotamente.

---

## 5. Relé 2 — Portão do Interfone (GPIO12)

Mesmo tipo do Relé 1, mas com função distinta.

### Como funciona no projeto
Ao receber `"ABRIR"` no tópico `home/interfone/abrir`, ativa o relé por **3 segundos** — tempo suficiente para acionar a fechadura elétrica do portão. O pulso mais longo (vs. 1s do Home Server) garante que a fechadura seja acionada corretamente.

---

## 6. Servo Motor — SG90 (GPIO5)

| Especificação | Detalhe |
|---|---|
| **Tipo** | Micro servo padrão (SG90 ou similar) |
| **Torque** | ~1.8 kg·cm |
| **Ângulo** | 0° a 180° |
| **Tensão** | 5V |

### Por que foi escolhido
- **Controle preciso de ângulo**: ideal para simular uma fechadura (trancado/destrancado).
- **Biblioteca `ESP32Servo`**: abstrai o PWM, permitindo comandar o ângulo com `servo.write(graus)`.
- **Baixo custo**: ~R\$ 15-20.

### Como funciona no projeto
Atua como **fechadura eletrônica**. Recebe comandos via MQTT:
- `"TRAVAR"` → servo vai para **0°** (trancado).
- `"DESTRANCAR"` → servo vai para **90°** (destrancado).

Após cada movimento, publica o estado atual no tópico `home/seguranca/porta/state`.

---

## 7. Sensor DHT22 (GPIO23)

| Especificação | Detalhe |
|---|---|
| **Tipo** | Sensor digital de temperatura e umidade |
| **Faixa temperatura** | -40°C a 80°C (±0.5°C) |
| **Faixa umidade** | 0 a 100% (±2% RH) |
| **Frequência máxima** | 0.5 Hz (uma leitura a cada 2s) |
| **Protocolo** | Single-bus proprietário |

### Por que foi escolhido
- **Precisão superior ao DHT11**: ±0.5°C vs ±2°C.
- **Biblioteca `DHT sensor library`** da Adafruit: simples e confiável.
- **Requisito do projeto**: monitorar temperatura do fogão — não precisa de alta frequência.

### Como funciona no projeto
- A cada **5 segundos** lê a temperatura e publica no tópico `home/cozinha/fogao/temperatura`.
- Se a temperatura ultrapassar **60°C**, publica `"CRITICO"` em `home/cozinha/fogao/alerta` — permite disparar alertas (notificação push, sirene, etc.).

---

## 8. Botão Push-Button (GPIO34)

| Especificação | Detalhe |
|---|---|
| **Tipo** | Botão momentâneo NA |
| **Configuração** | Pull-up externo (Wokwi) / interno |
| **GPIO** | 34 (entrada ADC, sem pull-up interno programável) |

### Por que foi escolhido
- **Sensor do interfone**: detecta quando alguém aperta a campainha.
- **Pull-up externo no GPIO34**: necessário porque o GPIO34 não possui pull-up interno via software.

### Como funciona no projeto
- Implementa **debounce por software** (50 ms) para evitar leituras espúrias.
- Quando pressionado (LOW por ≥ 50 ms), publica `"TOCANDO"` no tópico `home/interfone/status`.
- Só publica novamente quando o botão for solto e pressionado outra vez (evita spam).

---

## 9. WiFi (Conexão de Rede)

### Por que WiFi e não outra tecnologia
- **Já integrado ao ESP32**, sem hardware extra.
- **Alcance**: cobre uma residência típica.
- **Taxa de dados**: mais que suficiente para mensagens MQTT curtas.

### Como funciona no projeto
- Conexão **não-bloqueante**: não trava o loop enquanto conecta.
- Retenta automaticamente a cada **10 segundos** se a conexão cair.
- Usa `WiFi.mode(WIFI_STA)` para atuar apenas como estação (station).

---

## 10. MQTT (PubSubClient)

| Especificação | Detalhe |
|---|---|
| **Broker** | Mosquitto Docker (`localhost:1883`; no Wokwi, `10.0.2.2:1883`) |
| **Keep Alive** | 30 segundos |
| **QoS** | 0 (padrão do PubSubClient) |
| **Cliente ID** | `esp32_home_office` |

### Por que foi escolhido
- **Protocolo leve** (poucos bytes de overhead) — ideal para IoT.
- **Publicação/assinatura**: arquitetura desacoplada — qualquer dispositivo na rede pode assinar os tópicos.
- **Biblioteca `PubSubClient`**: compatível com qualquer WiFiClient, simples e testada.

### Como funciona no projeto
- **Subscribes**: o ESP32 ouve comandos em 4 tópicos (letreiro, server, porta, interfone).
- **Publishes**: envia temperatura, alerta, estado da fechadura e notificação do interfone.
- **Callback `mqttCallback()`**: roteia cada mensagem recebida para o componente apropriado.

---

## Resumo dos Tópicos MQTT

| Tópico | Tipo | Quem publica | Finalidade |
|---|---|---|---|
| `home/escritorio/status/cmd` | subscribe | App/sistema externo | Comando do letreiro (reuniao/foco/livre) |
| `home/escritorio/status` | publish | ESP32 | Estado atual do letreiro |
| `home/server/power/cmd` | subscribe | App/sistema externo | Ligar Home Server (pulso relé) |
| `home/server/power/state` | publish | ESP32 | Estado do pulso do Home Server (ON/OFF) |
| `home/seguranca/porta/cmd` | subscribe | App/sistema externo | Trancar/destrancar fechadura |
| `home/interfone/abrir` | subscribe | App/sistema externo | Abrir portão do interfone |
| `home/cozinha/fogao/temperatura` | publish | ESP32 | Leitura do DHT22 |
| `home/cozinha/fogao/alerta` | publish | ESP32 | Alerta crítico de temperatura |
| `home/seguranca/porta/state` | publish | ESP32 | Estado atual da fechadura |
| `home/interfone/status` | publish | ESP32 | Notificação de campainha |

---

## Diagrama de Conexões (Pinos)

| Componente | GPIO ESP32 | Função |
|---|---|---|
| LED Onboard | 2 | Status WiFi/MQTT |
| NeoPixel (Letreiro) | 19 | DIN (dados) |
| Relé 1 (Home Server) | 18 | Sinal de controle |
| Servo (Fechadura) | 5 | PWM |
| DHT22 (Temp. Fogão) | 23 | Sinal de dados |
| Botão (Interfone) | 34 | Entrada digital |
| Relé 2 (Portão) | 12 | Sinal de controle |
