# Home Office IoT

Automação residencial com ESP32, MQTT e Home Assistant

---

## Problema

Em um home office, algumas tarefas simples ainda dependem de ações manuais ou de presença física:

- indicar para outras pessoas se o usuário está em reunião, em foco ou disponível;
- ligar ou religar um home server;
- controlar uma fechadura ou acesso;
- receber eventos do interfone;
- monitorar riscos de temperatura na cozinha.

Essas ações ficam espalhadas e não possuem uma interface única de controle.

---

## Solução Proposta

A solução cria uma automação residencial centralizada usando um ESP32 conectado à rede WiFi.

O ESP32 lê sensores, aciona atuadores e troca mensagens MQTT com um broker Mosquitto. O Home Assistant funciona como painel de controle, permitindo comandar e visualizar os estados do sistema.

Com isso, o usuário consegue controlar o ambiente remotamente, acompanhar alertas e automatizar pequenas rotinas do home office.

---

## Arquitetura da Solução

Fluxo principal:

1. O Home Assistant envia comandos por MQTT.
2. O Mosquitto recebe e distribui as mensagens.
3. O ESP32 assina os tópicos de comando.
4. O firmware aciona LEDs, relés e servo motor conforme a mensagem recebida.
5. Sensores e estados são publicados de volta para o Home Assistant.

Componentes de software:

- Firmware ESP32 em C++/Arduino.
- Broker MQTT Mosquitto em Docker.
- Home Assistant em Docker.
- Simulação no Wokwi.

---

## Equipamentos Utilizados

| Equipamento | Função no projeto |
|---|---|
| ESP32 DevKit V1 | Controlador central da automação, com WiFi integrado |
| LED onboard | Indica o estado da conexão WiFi/MQTT |
| Fita NeoPixel WS2812B | Letreiro visual do escritório: livre, foco ou reunião |
| Relé 1 | Simula o botão power do home server |
| Relé 2 | Aciona o portão/interfone por pulso temporizado |
| Servo motor SG90 | Representa a fechadura eletrônica, travando ou destravando |
| Sensor DHT22 | Mede a temperatura do fogão/cozinha |
| Botão push-button | Simula o toque do interfone |

---

## Funções Automatizadas

Letreiro de reunião:

- vermelho: reunião;
- amarelo: foco;
- verde: livre.

Controle de acesso:

- comando para travar ou destravar a fechadura;
- comando para abrir o portão do interfone.

Monitoramento:

- leitura de temperatura a cada 5 segundos;
- alerta crítico quando a temperatura ultrapassa 60 °C;
- publicação do evento de interfone tocando.

---

## Comentários Sobre o Código

O firmware foi desenvolvido em C++ usando o framework Arduino para ESP32.

Principais bibliotecas:

- `WiFi`: conexão do ESP32 à rede;
- `PubSubClient`: comunicação MQTT;
- `Adafruit_NeoPixel`: controle da fita de LEDs;
- `DHT`: leitura do sensor de temperatura;
- `ESP32Servo`: controle do servo motor.

O código usa `millis()` em vez de `delay()`, permitindo executar várias tarefas no mesmo loop sem travar o sistema.

---

## Lógica Implementada

O callback MQTT interpreta as mensagens recebidas e direciona cada comando para o componente correto:

- `home/escritorio/status/cmd`: muda a cor do letreiro;
- `home/server/power/cmd`: gera um pulso no relé do home server;
- `home/seguranca/porta/cmd`: movimenta o servo da fechadura;
- `home/interfone/abrir`: aciona o relé do portão.

Também há publicação de estados para o Home Assistant, como temperatura, alerta de calor, estado da fechadura e status do interfone.

---

## Conclusão

A solução integra sensores, atuadores e uma interface web em uma arquitetura simples de IoT.

O uso de MQTT deixa os componentes desacoplados, o ESP32 executa o controle físico do ambiente e o Home Assistant centraliza a operação para o usuário.

O projeto pode ser testado em simulação pelo Wokwi ou adaptado para hardware real.
