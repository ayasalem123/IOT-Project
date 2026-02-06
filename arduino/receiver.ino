#include <Arduino.h>
#include <SoftwareSerial.h>
#include <WiFiS3.h>          // UNO R4 WiFi
#include <PubSubClient.h>    // MQTT
#include <Crypto.h>
#include <SHA256.h>

// =====================
// 1) CONFIG RESEAU
// =====================
// 👉 Solution recommandée: hotspot du PC (plus simple, pas de portail captif)
const char* ssid     = "HUAWEI P smart 2021";      // SSID du hotspot
const char* password = "0473853e5beb";       // MDP du hotspot

// IP du PC qui fait tourner Mosquitto + Node-RED
// (sur hotspot Windows: souvent 192.168.137.1)
const char* mqttServer = "192.168.43.49";
const int   mqttPort   = 1883;

// Topics
const char* topicSensors = "campus/coworking/sensors";
// Optionnel (si plus tard vous voulez piloter depuis Node-RED)
// const char* topicCmd     = "campus/coworking/command";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// =====================
// HMAC SHA256
// =====================
const char* HMAC_KEY = "CESI_2026_HMAC_SECRET";


// =====================
// 2) XBEE (ZigBee API)
// =====================
// ⚠️ Eviter 0/1 (réservés Serial). Choisir D2/D3 par ex.
#define XBEE_RX 0
#define XBEE_TX 1
SoftwareSerial xbeeSerial(XBEE_RX, XBEE_TX);
#define XBeePort xbeeSerial

// =====================
// 3) LED verte
// =====================
#define LED_PIN 8

struct Payload {
  uint8_t  id;
  uint32_t seq;
  uint32_t ts;
  uint16_t luxRaw;
  uint16_t distCm;
  uint8_t  motion;   // presence hold
  uint8_t  lightOn;  // décision
} __attribute__((packed));

static bool verifyChecksum(const uint8_t *frameData, uint16_t len, uint8_t checksum) {
  uint16_t sum = 0;
  for (uint16_t i = 0; i < len; i++) sum += frameData[i];
  return (((sum + checksum) & 0xFF) == 0xFF);
}

// API buffer
uint8_t  apiBuffer[260];
uint16_t apiIndex = 0;
bool     frameStarted = false;
uint16_t frameLen = 0;

static void setLed(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// =====================
// HMAC SHA256 FUNCTION
// =====================
static void hmacSHA256(const char* key, const char* msg, char* outHex) {
  SHA256 sha;
  uint8_t digest[32];

  uint8_t k0[64];
  memset(k0, 0, sizeof(k0));

  size_t keyLen = strlen(key);

  if (keyLen > 64) {
    sha.reset();
    sha.update((const uint8_t*)key, keyLen);
    sha.finalize(digest, 32);
    memcpy(k0, digest, 32);
  } else {
    memcpy(k0, key, keyLen);
  }

  uint8_t ipad[64], opad[64];
  for (int i = 0; i < 64; i++) {
    ipad[i] = k0[i] ^ 0x36;
    opad[i] = k0[i] ^ 0x5c;
  }

  // inner hash
  sha.reset();
  sha.update(ipad, 64);
  sha.update((const uint8_t*)msg, strlen(msg));
  sha.finalize(digest, 32);

  // outer hash
  sha.reset();
  sha.update(opad, 64);
  sha.update(digest, 32);
  sha.finalize(digest, 32);

  // hex
  for (int i = 0; i < 32; i++) {
    sprintf(outHex + (i * 2), "%02x", digest[i]);
  }
  outHex[64] = '\0';
}


// =====================
// 4) WIFI + MQTT
// =====================
static void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting WiFi to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("RX IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi NOT connected (timeout)");
  }
}

static void connectMQTT() {
  if (mqttClient.connected()) return;

  mqttClient.setServer(mqttServer, mqttPort);

  Serial.print("Connecting MQTT to ");
  Serial.print(mqttServer);
  Serial.print(":");
  Serial.println(mqttPort);

  // ClientID unique
  while (!mqttClient.connected()) {
    if (mqttClient.connect("RX_Coworking", "cesi", "cesi")) {
      Serial.println("MQTT connected");
      // Optionnel: subscribe si vous voulez recevoir des commandes plus tard
      // mqttClient.subscribe(topicCmd);
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retry in 2s");
      delay(2000);
    }
  }
}

// Optionnel: callback commandes (si vous activez subscribe)
// void mqttCallback(char* topic, byte* payload, unsigned int length) {
//   // Exemple: si payload "ON"/"OFF" -> LED
// }

static void publishPayloadMQTT(const Payload& pkt) {
  if (!mqttClient.connected()) return;

  char json[220];
  snprintf(json, sizeof(json),
    "{"
      "\"id\":%u,"
      "\"seq\":%lu,"
      "\"ts\":%lu,"
      "\"lux\":%u,"
      "\"dist\":%u,"
      "\"presence\":%u,"
      "\"lightOn\":%u"
    "}",
    pkt.id,
    (unsigned long)pkt.seq,
    (unsigned long)pkt.ts,
    pkt.luxRaw,
    pkt.distCm,
    pkt.motion,
    pkt.lightOn
  );
char hmacHex[65];
hmacSHA256(HMAC_KEY, json, hmacHex);

char payload[360];
snprintf(payload, sizeof(payload),
  "{"
    "\"data\":%s,"
    "\"hmac\":\"%s\""
  "}",
  json,
  hmacHex
);

  bool ok = mqttClient.publish(topicSensors, payload);
  Serial.print("MQTT publish ");
  Serial.print(ok ? "OK" : "FAIL");
  Serial.print(" -> ");
  Serial.println(json);
}

// =====================
// 5) ZigBee frame parsing
// =====================
static void parseFrame() {
  const uint8_t *frameData = &apiBuffer[3];
  uint8_t checksum = apiBuffer[3 + frameLen];

  if (!verifyChecksum(frameData, frameLen, checksum)) return;
  if (frameData[0] != 0x90) return; // ZigBee Receive Packet

  const uint16_t header = 12;
  if (frameLen < header + sizeof(Payload)) return;

  Payload pkt;
  memcpy(&pkt, &frameData[header], sizeof(Payload));

  // Action LED
  setLed(pkt.lightOn == 1);

  // Debug console
  Serial.print(F("RX | id="));      Serial.print(pkt.id);
  Serial.print(F(" | seq="));       Serial.print(pkt.seq);
  Serial.print(F(" | luxRaw="));    Serial.print(pkt.luxRaw);
  Serial.print(F(" | distCm="));    Serial.print(pkt.distCm);
  Serial.print(F(" | presence="));  Serial.print(pkt.motion);
  Serial.print(F(" | lightOn="));   Serial.println(pkt.lightOn);

  // >>> Publish MQTT (Node-RED recevra en direct)
  publishPayloadMQTT(pkt);
}

static void readAPI() {
  while (XBeePort.available()) {
    uint8_t b = (uint8_t)XBeePort.read();

    if (!frameStarted) {
      if (b == 0x7E) {
        frameStarted = true;
        apiIndex = 0;
        frameLen = 0;
        apiBuffer[apiIndex++] = b;
      }
      continue;
    }

    apiBuffer[apiIndex++] = b;

    if (apiIndex == 3) {
      frameLen = ((uint16_t)apiBuffer[1] << 8) | (uint16_t)apiBuffer[2];
      if (frameLen > (sizeof(apiBuffer) - 4)) {
        frameStarted = false; apiIndex = 0; frameLen = 0;
      }
    }

    if (frameLen > 0) {
      uint16_t totalNeeded = 3 + frameLen + 1;
      if (apiIndex >= totalNeeded) {
        parseFrame();
        frameStarted = false; apiIndex = 0; frameLen = 0;
      }
    }

    if (apiIndex >= sizeof(apiBuffer)) {
      frameStarted = false; apiIndex = 0; frameLen = 0;
    }
  }
}

// =====================
// setup / loop
// =====================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

  // ZigBee serial
  XBeePort.begin(9600);

  // WiFi + MQTT
  connectWiFi();
  connectMQTT();

  Serial.println(F("=== RX: XBee API -> LED + MQTT publish ==="));
  Serial.println(F("Waiting frames 0x90...\n"));
}

void loop() {
  // Maintien connexions
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  // Lecture ZigBee
  readAPI();
}