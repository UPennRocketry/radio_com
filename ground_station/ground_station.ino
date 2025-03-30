#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS  8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 433.0

#define CAMERA_BUTTON_PIN 5
#define SPEAKER_BUTTON_PIN 10

RH_RF95 rf95(RFM95_CS, RFM95_INT);

unsigned long lastCommandSendTime = 0;
const int commandRepeatInterval = 50; // Send commands every 50ms during RX window
const int commandWindowDuration = 200; // Total 200ms listening window
unsigned long commandWindowStart = 0;
bool inCommandWindow = false;

unsigned long lastTelemetryTime = 0;
const int telemetryInterval = 100; // Every 100ms
int packetCount = 0;
const int packetsBeforePause = 10;

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println("LoRa TX (Feather M0) with Switch-Based Commands + Telemetry Reception");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    Serial.println("LoRa init failed");
    while (1);
  }

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }

  rf95.setTxPower(13, false);
  rf95.setModeRx();

  pinMode(CAMERA_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SPEAKER_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  unsigned long now = millis();

  // Telemetry reception
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      digitalWrite(LED_BUILTIN, HIGH);

      buf[len] = '\0';
      Serial.print("Telemetry: ");
      Serial.println((char*)buf);

      digitalWrite(LED_BUILTIN, LOW);

      packetCount++;
      if (packetCount >= packetsBeforePause) {
        packetCount = 0;
        inCommandWindow = true;
        commandWindowStart = now;
        lastCommandSendTime = 0; // Reset so we send immediately
      }
    } else {
      Serial.println("Receive failed");
    }
  }

  // During RX window, send command if switch is ON
  if (inCommandWindow) {
    if (now - commandWindowStart < commandWindowDuration) {
      if (now - lastCommandSendTime >= commandRepeatInterval) {
        if (digitalRead(CAMERA_BUTTON_PIN) == LOW) {
          sendCommand("CAMERA_ON");
        }

        if (digitalRead(SPEAKER_BUTTON_PIN) == LOW) {
          sendCommand("SPEAKER_ON");
        } else {
          sendCommand("SPEAKER_OFF");
        }

        lastCommandSendTime = now;
      }
    } else {
      inCommandWindow = false;
    }
  }
}

void sendCommand(const char* command) {
  rf95.setModeTx();
  Serial.print("Sending command: ");
  Serial.println(command);
  rf95.send((uint8_t*)command, strlen(command));
  rf95.waitPacketSent();

  rf95.setModeRx();
  if (rf95.waitAvailableTimeout(200)) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      Serial.print("Ack: ");
      Serial.println((char*)buf);
    }
  }
}
