
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// -------------------------------
// PINOS
// -------------------------------
#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 19

// -------------------------------
// CALIBRACAO DA LIXEIRA
// -------------------------------
const float DISTANCIA_VAZIA = 15.18;
const float DISTANCIA_CHEIA = 4.44;

// -------------------------------
// WIFI
// -------------------------------
const char* WIFI_SSID = "iPhone de Melissa";
const char* WIFI_PASSWORD = "fegraci19";

// -------------------------------
// HIVEMQ CLOUD
// -------------------------------
const char* MQTT_HOST = "849791f6db184775831e56d079e429f6.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;

const char* MQTT_USER = "pinkbin_esp32";
const char* MQTT_PASSWORD = "MelMat2026";

// Topico para envio dos dados
const char* MQTT_TOPIC = "pinkbin/dados";

// -------------------------------
// CONEXAO MQTT SEGURA
// -------------------------------
WiFiClientSecure espClient;
PubSubClient client(espClient);

// -------------------------------
// CONECTAR AO WIFI
// -------------------------------
void conectarWiFi() {

  Serial.println("Conectando ao Wi-Fi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("[OK] Wi-Fi conectado!");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// -------------------------------
// CONECTAR AO HIVEMQ
// -------------------------------
void conectarMQTT() {

  while (!client.connected()) {

    Serial.println("Conectando ao HiveMQ...");

    if (client.connect("PinkBinESP32", MQTT_USER, MQTT_PASSWORD)) {

      Serial.println("[OK] Conectado ao HiveMQ!");

    } else {

      Serial.print("[ERRO] MQTT: ");
      Serial.println(client.state());

      delay(3000);
    }
  }
}

// -------------------------------
// MEDIR DISTANCIA
// -------------------------------
float medirDistancia() {

  float soma = 0;
  int leiturasValidas = 0;

  for (int i = 0; i < 5; i++) {

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(3);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    long duracao = pulseIn(ECHO_PIN, HIGH, 50000);

    if (duracao > 0) {

      float distancia = duracao * 0.0343 / 2;

      if (distancia >= 2 && distancia <= 400) {
        soma += distancia;
        leiturasValidas++;
      }
    }

    delay(60);
  }

  if (leiturasValidas < 3) {
    return -1;
  }

  return soma / leiturasValidas;
}

// -------------------------------
// OBTER DATA E HORA
// -------------------------------
void obterDataHora(char* resultado, size_t tamanho) {

  struct tm informacaoTempo;

  if (getLocalTime(&informacaoTempo, 10)) {

    strftime(resultado, tamanho, "%Y-%m-%d %H:%M:%S",
             &informacaoTempo);

  } else {

    snprintf(resultado, tamanho, "indisponivel");
  }
}

// -------------------------------
// ENVIAR DADOS AO HIVEMQ
// -------------------------------
void enviarDados(float distancia, float ocupacao,
                 const char* status) {

  char dataHora[25];
  obterDataHora(dataHora, sizeof(dataHora));

  char mensagem[256];

  snprintf(mensagem, sizeof(mensagem),
    "{\"distancia\":%.2f,\"ocupacao\":%.2f,\"status\":\"%s\",\"timestamp\":\"%s\"}",
    distancia, ocupacao, status, dataHora);

  if (client.publish(MQTT_TOPIC, mensagem)) {

    Serial.println("[OK] Dados enviados ao HiveMQ!");
    Serial.println(mensagem);

  } else {

    Serial.println("[ERRO] Falha ao enviar dados MQTT!");
  }
}

// -------------------------------
// CONFIGURACAO INICIAL
// -------------------------------
void setup() {

  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("==============================");
  Serial.println("       PINKBIN INICIADA");
  Serial.println("==============================");

  conectarWiFi();

  // Horario de Brasilia: UTC-3
  configTime(-3 * 3600, 0,
             "pool.ntp.org",
             "time.cloudflare.com");

  // Para teste em sala de aula.
  // A conexao usa TLS, mas nao valida o certificado.
  espClient.setInsecure();

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setBufferSize(256);

  conectarMQTT();
}

// -------------------------------
// LOOP PRINCIPAL
// -------------------------------
void loop() {

  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  if (!client.connected()) {
    conectarMQTT();
  }

  client.loop();

  float distancia = medirDistancia();

  if (distancia < 0) {

    Serial.println("[ERRO] Falha na leitura do sensor!");
    digitalWrite(BUZZER_PIN, LOW);

  } else {

    float ocupacao =
      ((DISTANCIA_VAZIA - distancia) /
      (DISTANCIA_VAZIA - DISTANCIA_CHEIA)) * 100;

    ocupacao = constrain(ocupacao, 0, 100);

    const char* status;

    Serial.println("------------------------------");

    Serial.print("Distancia: ");
    Serial.print(distancia);
    Serial.println(" cm");

    Serial.print("Ocupacao: ");
    Serial.print(ocupacao);
    Serial.println("%");

    if (ocupacao >= 80) {

      status = "CHEIA";
      Serial.println("[ALERTA] LIXEIRA CHEIA!");

      for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(300);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
      }

    } else if (ocupacao >= 50) {

      status = "ATENCAO";
      Serial.println("[ATENCAO] LIXEIRA NA METADE!");
      digitalWrite(BUZZER_PIN, LOW);

    } else {

      status = "NORMAL";
      Serial.println("[STATUS] LIXEIRA NORMAL!");
      digitalWrite(BUZZER_PIN, LOW);
    }

    enviarDados(distancia, ocupacao, status);
  }

  delay(1000);
}
