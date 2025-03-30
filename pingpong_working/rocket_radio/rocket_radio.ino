// RocketPing.ino
#include <SPI.h>
#include <RH_RF95.h>
#include <math.h>

// LoRa pins
#define RFM95_CS   8
#define RFM95_RST  4
#define RFM95_INT  3

#define RF95_FREQ  433.0

// Outputs
#define LED 13
#define SPEAKER_PIN 12
#define CAMERA_PIN 6

// Radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Telemetry
static float angle = 0.0;
const float angleIncrement = 0.1;
const float amplitude      = 100.0;

// Timing
unsigned long lastSendTime   = 0;
unsigned long telemetryDelay = 1000; // 1 second between pings

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(CAMERA_PIN, OUTPUT);

  // Speaker off by default (active low)
  digitalWrite(SPEAKER_PIN, HIGH);
  // Camera off by default
  digitalWrite(CAMERA_PIN, LOW);

  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);
  while (!Serial); // for SAMD
  delay(100);

  Serial.println("Rocket Ping-Pong: Rocket Side");

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

  // High TX power for rocket
  rf95.setTxPower(23, false);

  Serial.println("Rocket ready to ping...");
}

void loop() {
  unsigned long now = millis();
  // Send telemetry every 1s (adjust as needed)
  if (now - lastSendTime >= telemetryDelay) {
    sendTelemetry();
    lastSendTime = now;

    // After sending, wait for ground station reply
    waitForGroundReply();
  }
}

void sendTelemetry() {
  // Generate telemetry data
  float velocity     = amplitude * sin(angle);
  float altitude     = amplitude * sin(angle + 1.0);
  float temperature  = amplitude * sin(angle + 2.0);
  float pressure     = amplitude * sin(angle + 3.0);
  angle += angleIncrement;

  // Format packet
  char packet[100];
  snprintf(packet, sizeof(packet),
           "Velocity:%.2f,Altitude:%.2f,Temp:%.2f,Pressure:%.2f",
            velocity, altitude, temperature, pressure);

  Serial.print("Sending telemetry: ");
  Serial.println(packet);

  // Send
  rf95.setModeTx();
  rf95.send((uint8_t*)packet, strlen(packet));
  rf95.waitPacketSent();
  rf95.setModeRx(); // ready to receive reply
}

void waitForGroundReply() {
  unsigned long start = millis();
  bool gotReply = false;

  // We'll wait up to 2 seconds for a reply
  while (millis() - start < 2000) {
    if (rf95.available()) {
      // We got a reply
      gotReply = true;
      handleGroundCommand();
      break;
    }
  }

  if (!gotReply) {
    Serial.println("No command reply from ground station.");
  }
}

void handleGroundCommand() {
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (rf95.recv(buf, &len)) {
    buf[len] = '\0'; // Null-terminate
    String command = String((char*)buf);
    command.trim();

    Serial.print("Received command: ");
    Serial.println(command);

    // Act on the command
    if (command == "SPEAKER_ON") {
      Serial.println(">> Speaker ON");
      digitalWrite(SPEAKER_PIN, LOW);
    } else if (command == "SPEAKER_OFF") {
      Serial.println(">> Speaker OFF");
      digitalWrite(SPEAKER_PIN, HIGH);
    } else if (command == "CAMERA_ON") {
      Serial.println(">> Camera ON pulse");
      digitalWrite(CAMERA_PIN, HIGH);
      delay(500);
      digitalWrite(CAMERA_PIN, LOW);
    } else {
      Serial.println(">> Unknown command");
    }

    // Optional LED blink
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
  } else {
    Serial.println("Receive failed on command packet");
  }
}
