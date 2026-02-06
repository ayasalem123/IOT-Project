#include <Arduino.h>
#include <SoftwareSerial.h>

// ===== XBee (Uno/Nano) =====
#define XBEE_RX 0
#define XBEE_TX 1
SoftwareSerial xbeeSerial(XBEE_RX, XBEE_TX);
#define XBeePort xbeeSerial

// ===== Capteurs =====
#define LIGHT_PIN A0

// HC-SR04
#define TRIG_PIN 9
#define ECHO_PIN 8

// ===== Logique =====
#define NODE_ID 1
#define PERIOD_MS 200UL

// présence si distance < seuil (cm)
#define PRESENCE_CM        100
#define PRESENCE_HOLD_MS   2000UL

// luminosité
#define LUX_ON_THRESHOLD   350
#define LUX_OFF_THRESHOLD  450

struct Payload {
  uint8_t  id;
  uint32_t seq;
  uint32_t ts;
  uint16_t luxRaw;
  uint16_t distCm;
  uint8_t  motion;
  uint8_t  lightOn;
} __attribute__((packed));

static uint8_t xbeeChecksum(const uint8_t *frameData, uint16_t len) {
  uint16_t sum = 0;
  for (uint16_t i = 0; i < len; i++) sum += frameData[i];
  return (uint8_t)(0xFF - (sum & 0xFF));
}

static uint16_t readLightAvg(uint8_t n = 8) {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < n; i++) {
    acc += (uint16_t)analogRead(LIGHT_PIN);
    delay(2);
  }
  return (uint16_t)(acc / n);
}

static uint16_t readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (dur == 0) return 0;
  return (uint16_t)(dur / 58UL);
}

uint32_t seqCounter = 0;
uint32_t lastSend = 0;

uint32_t lastPresenceTime = 0;
bool lightState = false;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {}

  pinMode(LIGHT_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  XBeePort.begin(9600);

  Serial.println(F("=== TX: Light + HC-SR04 -> Decision -> XBee API ==="));
  Serial.println(F("Pins: XBee(2,3) Light(A0) TRIG(9) ECHO(8) | AP=1 BD=9600"));
}

void loop() {
  uint16_t distCm = readDistanceCm();
  bool presenceNow = (distCm != 0 && distCm < PRESENCE_CM);

  if (presenceNow) lastPresenceTime = millis();
  bool presenceHold = (millis() - lastPresenceTime) < PRESENCE_HOLD_MS;

  uint16_t luxRaw = readLightAvg(8);

  bool isDark;
  if (!lightState) isDark = (luxRaw <= LUX_ON_THRESHOLD);
  else             isDark = !(luxRaw >= LUX_OFF_THRESHOLD);

  bool shouldLightOn = presenceHold && isDark;
  lightState = shouldLightOn;

  uint32_t now = millis();
  if (now - lastSend >= PERIOD_MS) {
    lastSend = now;

    Payload pkt;
    pkt.id = NODE_ID;
    pkt.seq = seqCounter++;
    pkt.ts = millis();
    pkt.luxRaw = luxRaw;
    pkt.distCm = distCm;
    pkt.motion = presenceHold ? 1 : 0;
    pkt.lightOn = lightState ? 1 : 0;

    uint8_t frameData[14 + sizeof(Payload)];
    uint16_t idx = 0;

    frameData[idx++] = 0x10;
    frameData[idx++] = 0x01;

    // dest64 broadcast
    frameData[idx++] = 0x00; frameData[idx++] = 0x00; frameData[idx++] = 0x00; frameData[idx++] = 0x00;
    frameData[idx++] = 0x00; frameData[idx++] = 0x00; frameData[idx++] = 0xFF; frameData[idx++] = 0xFF;

    // dest16
    frameData[idx++] = 0xFF; frameData[idx++] = 0xFE;

    frameData[idx++] = 0x00;
    frameData[idx++] = 0x00;

    memcpy(&frameData[idx], &pkt, sizeof(Payload));
    idx += sizeof(Payload);

    uint16_t length = idx;
    uint8_t checksum = xbeeChecksum(frameData, length);

    XBeePort.write(0x7E);
    XBeePort.write((uint8_t)(length >> 8));
    XBeePort.write((uint8_t)(length & 0xFF));
    XBeePort.write(frameData, length);
    XBeePort.write(checksum);

    Serial.print(F("TX | luxRaw="));   Serial.print(pkt.luxRaw);
    Serial.print(F(" | distCm="));    Serial.print(pkt.distCm);
    Serial.print(F(" | presence="));  Serial.print(pkt.motion);
    Serial.print(F(" | lightOn="));   Serial.println(pkt.lightOn);
  }
}