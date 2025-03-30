#include <SPI.h>
#include <RH_RF95.h>
#include <math.h>

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 433.0
#define LED 13
#define SPEAKER_PIN 12
#define CAMERA_PIN 6

RH_RF95 rf95(RFM95_CS, RFM95_INT);

const int telemetryInterval = 100;  // ms between telemetry packets
const int commandCheckInterval = 10; // every 10 packets, open RX window
int packetCount = 0;

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(CAMERA_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, HIGH); // speaker off (active low)
  digitalWrite(CAMERA_PIN, LOW);

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);
  while (!Serial); // Wait for Serial Monitor on SAMD21
  delay(100);

  Serial.println("LoRa RX (Rocket) Ready");

  // Manual reset
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

  // Max Tx power for rocket
  rf95.setTxPower(23, false);

  // Start in TX mode (sending telemetry)
  rf95.setModeTx();
}

void loop() {
  static float angle = 0.0;
  const float angleIncrement = 0.1;
  const float amplitude = 100.0;

  // Generate telemetry data
  float velocity     = amplitude * sin(angle);
  float altitude     = amplitude * sin(angle + 1.0);
  float temperature  = amplitude * sin(angle + 2.0);
  float pressure     = amplitude * sin(angle + 3.0);
  angle += angleIncrement;

  char radiopacket[100];
  snprintf(radiopacket, sizeof(radiopacket),
           "Velocity:%.2f,Altitude:%.2f,Temperature:%.2f,Pressure:%.2f",
            velocity, altitude, temperature, pressure);

  Serial.print("Sending: ");
  Serial.println(radiopacket);

  // Send telemetry
  rf95.send((uint8_t*)radiopacket, strlen(radiopacket));
  rf95.waitPacketSent();

  packetCount++;

  // Every N packets, open a 200ms RX window
  if (packetCount >= commandCheckInterval) {
    packetCount = 0;
    // Switch to RX mode
    rf95.setModeRx();
    delay(5); // let radio stabilize

    Serial.println("Listening for command...");

    unsigned long start = millis();
    bool receivedPacket = false;

    while (millis() - start < 200) {
      if (rf95.available()) {
        receivedPacket = true;
        break;
      }
    }

    Serial.println("checking");

    if (receivedPacket) {
      Serial.println("available");
      uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
      uint8_t len = sizeof(buf);

      if (rf95.recv(buf, &len)) {
        String command = String((char*)buf);
        command.trim();
        Serial.print("Received: ");
        Serial.println(command);

        if (command == "SPEAKER_ON") {
          Serial.println(">> Turning speaker ON");
          digitalWrite(SPEAKER_PIN, LOW); // active low
        } else if (command == "SPEAKER_OFF") {
          Serial.println(">> Turning speaker OFF");
          digitalWrite(SPEAKER_PIN, HIGH);
        } else if (command == "CAMERA_ON") {
          Serial.println(">> Pulsing camera pin");
          digitalWrite(CAMERA_PIN, HIGH);
          delay(500);
          digitalWrite(CAMERA_PIN, LOW);
        }

        // Acknowledge
        const char* reply = "ACK";
        rf95.send((uint8_t*)reply, strlen(reply));
        rf95.waitPacketSent();
      }
    } else {
      Serial.println("No command received.");
    }

    // Switch back to TX mode
    rf95.setModeTx();
  }

  delay(telemetryInterval);
}
