#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ============================================================================
// CONFIGURACAO - Edite antes de compilar
// ============================================================================

#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASSWORD   ""

#define MQTT_SERVER     "host.wokwi.internal"  // Wokwi + Private IoT Gateway. Para HW real, use o IP local do broker.
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASSWORD   ""
#define MQTT_CLIENT_ID  "esp32_home_office"

// ============================================================================
// MAPEAMENTO DE PINOS
// ============================================================================

#define PIN_LED           2   // LED onboard (indicador WiFi/MQTT)
#define PIN_NEO_PIXEL     19  // Fita NeoPixel - Letreiro de reuniao
#define PIN_RELE_1        18  // Rele 1 - Pulso power do Home Server
#define PIN_SERVO         5   // Servo motor - Fechadura eletronica
#define PIN_DHT           23  // DHT22 - Temperatura do fogao
#define PIN_BOTAO         34  // Botao do interfone (INPUT c/ resistor pull-up externo)
#define PIN_RELE_2        12  // Rele 2 - Portao do interfone

// ============================================================================
// TOPICOS MQTT (subscribe / publish)
// ============================================================================

static const char T_FOCO_CMD[]          = "home/escritorio/status/cmd";
static const char T_FOCO_STATE[]        = "home/escritorio/status";
static const char T_SERVER_POWER_CMD[]  = "home/server/power/cmd";
static const char T_SERVER_POWER_STATE[] = "home/server/power/state";
static const char T_PORTA_CMD[]         = "home/seguranca/porta/cmd";
static const char T_PORTA_STATE[]       = "home/seguranca/porta/state";
static const char T_TEMPERATURA[]       = "home/cozinha/fogao/temperatura";
static const char T_ALERTA[]            = "home/cozinha/fogao/alerta";
static const char T_INTERFONE[]         = "home/interfone/status";
static const char T_INTERFONE_ABRIR[]   = "home/interfone/abrir";

// ============================================================================
// CONSTANTES
// ============================================================================

#define DHT_TIPO            DHT22
#define NEO_PIXEL_COUNT     8
#define TEMP_LIMITE         60.0f
#define SERVO_TRANCADO      0
#define SERVO_DESTRANCADO   90

#define DHT_INTERVALO_MS    5000
#define RELE1_PULSO_MS      1000
#define RELE2_PULSO_MS      3000
#define BOTAO_DEBOUNCE_MS   50
#define LED_BLINK_MS        500
#define INTERFONE_TOQUE_MS  8000

// ============================================================================
// OBJETOS GLOBAIS
// ============================================================================

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Adafruit_NeoPixel neoPixel(NEO_PIXEL_COUNT, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800);
DHT dht(PIN_DHT, DHT_TIPO);
Servo servo;

// ============================================================================
// VARIAVEIS DE ESTADO (millis)
// ============================================================================

bool   wifiConnected   = false;
bool   mqttConnected   = false;
bool   ledState        = false;
unsigned long lastLedToggle = 0;

// DHT22
unsigned long lastDhtRead = 0;

// Rele 1 (pulso do power)
bool   rele1Active = false;
unsigned long rele1StartMs = 0;

// Rele 2 (portao interfone)
bool   rele2Active = false;
unsigned long rele2StartMs = 0;

// WiFi (reconexao nao-bloqueante)
unsigned long lastWifiAttempt = 0;
const unsigned long WIFI_RETRY_MS = 10000;

// Botao interfone (debounce)
bool   lastButtonRaw = HIGH;
unsigned long lastButtonChangeMs = 0;
bool   botaoFoiPublicado = false;
bool   interfoneTocando = false;
unsigned long interfoneStartMs = 0;

// Servo
int   currentServoPos = SERVO_TRANCADO;
bool  servoPositionChanged = false;

// NeoPixel
bool  neoPixelChanged = false;
uint32_t neoPixelTargetColor = 0;
const char* currentOfficeStatus = "livre";

// ============================================================================
// PROTOTIPOS
// ============================================================================

void conectarWiFi();
void conectarMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setNeoPixelColor(uint32_t color);
void atualizarLED();
void lerDHT22();
void checarRele1();
void checarRele2();
void checarBotao();
void atualizarInterfone();
void atualizarServo();
void aplicarStatusEscritorio(const String& status, bool publishState);
void publicarEstadosIniciais();
void publicarEstadoFechadura();
void publicarEstadoServidor(const char* estado);
void iniciarToqueInterfone();
void encerrarToqueInterfone();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);

    // Pinos
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    pinMode(PIN_RELE_1, OUTPUT);
    digitalWrite(PIN_RELE_1, LOW);

    pinMode(PIN_RELE_2, OUTPUT);
    digitalWrite(PIN_RELE_2, LOW);

    pinMode(PIN_BOTAO, INPUT);

    // NeoPixel
    neoPixel.begin();
    neoPixel.setBrightness(128);
    neoPixel.clear();
    neoPixel.show();
    setNeoPixelColor(neoPixel.Color(0, 255, 0));

    // DHT22
    dht.begin();

    // Servo
    servo.attach(PIN_SERVO);
    servo.write(currentServoPos);

    // MQTT
    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setKeepAlive(30);

    // WiFi
    WiFi.mode(WIFI_STA);
    conectarWiFi();
}

// ============================================================================
// LOOP PRINCIPAL (sem delay())
// ============================================================================

void loop() {
    unsigned long now = millis();

    // 1. Garantir WiFi conectado (nao-bloqueante)
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        mqttConnected = false;
        if (now - lastWifiAttempt >= WIFI_RETRY_MS) {
            conectarWiFi();
        }
    } else {
        if (!wifiConnected) {
            wifiConnected = true;
            Serial.printf("[WiFi] Conectado! IP: %s\r\n", WiFi.localIP().toString().c_str());
        }
    }

    // 2. Garantir MQTT conectado
    if (wifiConnected) {
        if (!mqtt.connected()) {
            mqttConnected = false;
            conectarMQTT();
        } else {
            if (!mqttConnected) {
                mqttConnected = true;
                Serial.println("[MQTT] Conectado!");
            }
            mqtt.loop();
        }
    }

    // 3. LED de status
    atualizarLED();

    // 4. Ler DHT22 a cada 5s
    lerDHT22();

    // 5. Gerenciar pulso do Rele 1
    checarRele1();

    // 6. Gerenciar pulso do Rele 2
    checarRele2();

    // 7. Ler botao do interfone (com debounce)
    checarBotao();

    // 8. Atualizar estado temporizado do interfone
    atualizarInterfone();

    // 9. Atualizar servo se necessario
    atualizarServo();

    // 10. Atualizar NeoPixel se necessario
    if (neoPixelChanged) {
        neoPixel.fill(neoPixelTargetColor);
        neoPixel.show();
        neoPixelChanged = false;
    }
}

// ============================================================================
// FUNCOES DE CONEXAO
// ============================================================================

void conectarWiFi() {
    Serial.printf("[WiFi] Conectando a %s ...\r\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
    lastWifiAttempt = millis();
}

void conectarMQTT() {
    bool ok = false;
    if (strlen(MQTT_USER) > 0) {
        ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
    } else {
        ok = mqtt.connect(MQTT_CLIENT_ID);
    }

    if (ok) {
        mqtt.subscribe(T_FOCO_CMD);
        mqtt.subscribe(T_SERVER_POWER_CMD);
        mqtt.subscribe(T_PORTA_CMD);
        mqtt.subscribe(T_INTERFONE_ABRIR);
        publicarEstadosIniciais();
        Serial.println("[MQTT] Subscribed to all topics");
    } else {
        Serial.printf("[MQTT] Falha (rc=%d)\r\n", mqtt.state());
    }
}

// ============================================================================
// CALLBACK MQTT
// ============================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char msg[length + 1];
    memcpy(msg, payload, length);
    msg[length] = '\0';

    String t = String(topic);
    String v = String(msg);

    Serial.printf("[MQTT] << %s -> %s\r\n", topic, msg);

    // --- AER 1: Letreiro de reuniao (NeoPixel) ---
    if (t == T_FOCO_CMD) {
        aplicarStatusEscritorio(v, true);
    }

    // --- AER 2: Controle do Home Server (Rele 1) ---
    if (t == T_SERVER_POWER_CMD) {
        if (v == "LIGAR") {
            if (!rele1Active) {
                rele1Active = true;
                rele1StartMs = millis();
                digitalWrite(PIN_RELE_1, HIGH);
                publicarEstadoServidor("ON");
                Serial.println("[Rele1] Pulso power ON");
            }
        } else if (v == "DESLIGAR") {
            rele1Active = false;
            digitalWrite(PIN_RELE_1, LOW);
            publicarEstadoServidor("OFF");
            Serial.println("[Rele1] Pulso power OFF");
        }
    }

    // --- AER 3: Fechadura da porta (Servo) ---
    if (t == T_PORTA_CMD) {
        if (v == "TRAVAR") {
            currentServoPos = SERVO_TRANCADO;
            servoPositionChanged = true;
        } else if (v == "DESTRANCAR") {
            currentServoPos = SERVO_DESTRANCADO;
            servoPositionChanged = true;
        }
    }

    // --- AER 5: Portao do interfone (Rele 2) ---
    if (t == T_INTERFONE_ABRIR && v == "ABRIR") {
        if (!rele2Active) {
            rele2Active = true;
            rele2StartMs = millis();
            digitalWrite(PIN_RELE_2, HIGH);
            encerrarToqueInterfone();
            Serial.println("[Rele2] Portao ABRIR");
        }
    }
}

// ============================================================================
// LED DE STATUS
// ============================================================================

void atualizarLED() {
    unsigned long now = millis();

    if (mqttConnected) {
        // Conexao OK - LED aceso fixo
        if (!ledState) {
            ledState = true;
            digitalWrite(PIN_LED, HIGH);
        }
    } else {
        // Sem conexao - LED piscando
        if (now - lastLedToggle >= LED_BLINK_MS) {
            lastLedToggle = now;
            ledState = !ledState;
            digitalWrite(PIN_LED, ledState ? HIGH : LOW);
        }
    }
}

// ============================================================================
// NEOPIXEL
// ============================================================================

void setNeoPixelColor(uint32_t color) {
    neoPixelTargetColor = color;
    neoPixelChanged = true;
}

void aplicarStatusEscritorio(const String& status, bool publishState) {
    if (status == "reuniao") {
        currentOfficeStatus = "reuniao";
        setNeoPixelColor(neoPixel.Color(255, 0, 0));     // Vermelho
    } else if (status == "foco") {
        currentOfficeStatus = "foco";
        setNeoPixelColor(neoPixel.Color(255, 255, 0));   // Amarelo
    } else if (status == "livre") {
        currentOfficeStatus = "livre";
        setNeoPixelColor(neoPixel.Color(0, 255, 0));     // Verde
    } else {
        Serial.printf("[Letreiro] Status invalido: %s\r\n", status.c_str());
        return;
    }

    if (publishState && mqtt.connected()) {
        mqtt.publish(T_FOCO_STATE, currentOfficeStatus, true);
    }
}

void publicarEstadoServidor(const char* estado) {
    if (mqtt.connected()) {
        mqtt.publish(T_SERVER_POWER_STATE, estado, true);
    }
}

void publicarEstadoFechadura() {
    if (!mqtt.connected()) return;

    const char* estado = (currentServoPos == SERVO_TRANCADO) ? "TRANCADO" : "DESTRANCADO";
    mqtt.publish(T_PORTA_STATE, estado, true);
}

void publicarEstadosIniciais() {
    mqtt.publish(T_FOCO_STATE, currentOfficeStatus, true);
    publicarEstadoServidor(rele1Active ? "ON" : "OFF");
    publicarEstadoFechadura();
    mqtt.publish(T_ALERTA, "NORMAL", true);
    mqtt.publish(T_INTERFONE, interfoneTocando ? "TOCANDO" : "SILENCIO", true);
}

// ============================================================================
// DHT22 - LEITURA DE TEMPERATURA
// ============================================================================

void lerDHT22() {
    unsigned long now = millis();
    if (now - lastDhtRead < DHT_INTERVALO_MS) return;
    lastDhtRead = now;

    float temp = dht.readTemperature();
    if (isnan(temp)) {
        Serial.println("[DHT22] Falha na leitura");
        return;
    }

    Serial.printf("[DHT22] Temperatura: %.1f C\r\n", temp);

    if (!mqtt.connected()) return;

    // Publica temperatura
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", temp);
    mqtt.publish(T_TEMPERATURA, buf, true);

    // Alerta critico
    if (temp > TEMP_LIMITE) {
        mqtt.publish(T_ALERTA, "CRITICO", true);
        Serial.println("[DHT22] ALERTA CRITICO!");
    } else {
        mqtt.publish(T_ALERTA, "NORMAL", true);
    }
}

// ============================================================================
// RELE 1 - PULSO POWER HOME SERVER
// ============================================================================

void checarRele1() {
    if (!rele1Active) return;

    if (millis() - rele1StartMs >= RELE1_PULSO_MS) {
        digitalWrite(PIN_RELE_1, LOW);
        rele1Active = false;
        publicarEstadoServidor("OFF");
        Serial.println("[Rele1] Pulso power OFF");
    }
}

// ============================================================================
// RELE 2 - PORTAO INTERFONE
// ============================================================================

void checarRele2() {
    if (!rele2Active) return;

    if (millis() - rele2StartMs >= RELE2_PULSO_MS) {
        digitalWrite(PIN_RELE_2, LOW);
        rele2Active = false;
        Serial.println("[Rele2] Portao FECHADO");
    }
}

// ============================================================================
// BOTAO DO INTERFONE
// ============================================================================

void checarBotao() {
    unsigned long now = millis();
    bool raw = digitalRead(PIN_BOTAO);

    if (raw != lastButtonRaw) {
        lastButtonChangeMs = now;
        lastButtonRaw = raw;
    }

    bool botaoPressionado = (raw == LOW) && ((now - lastButtonChangeMs) >= BOTAO_DEBOUNCE_MS);

    if (botaoPressionado && !botaoFoiPublicado) {
        iniciarToqueInterfone();
        botaoFoiPublicado = true;
    }

    // Permite detectar um novo toque quando o botao for solto.
    if (raw == HIGH && botaoFoiPublicado) {
        botaoFoiPublicado = false;
    }
}

void iniciarToqueInterfone() {
    interfoneTocando = true;
    interfoneStartMs = millis();

    if (wifiConnected && mqtt.connected()) {
        mqtt.publish(T_INTERFONE, "TOCANDO", true);
    }

    Serial.println("[Interfone] TOCANDO!");
}

void encerrarToqueInterfone() {
    if (!interfoneTocando) return;

    interfoneTocando = false;
    if (wifiConnected && mqtt.connected()) {
        mqtt.publish(T_INTERFONE, "SILENCIO", true);
    }

    Serial.println("[Interfone] SILENCIO");
}

void atualizarInterfone() {
    if (!interfoneTocando) return;

    if (millis() - interfoneStartMs >= INTERFONE_TOQUE_MS) {
        encerrarToqueInterfone();
    }
}

// ============================================================================
// SERVO MOTOR - FECHADURA ELETRONICA
// ============================================================================

void atualizarServo() {
    if (!servoPositionChanged) return;
    servoPositionChanged = false;

    servo.write(currentServoPos);
    Serial.printf("[Servo] Posicao: %d\r\n", currentServoPos);

    publicarEstadoFechadura();
}
