// GroundStationPong.ino
#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS  8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 433.0

// Example: Read buttons to decide if speaker is ON or OFF
#define SPEAKER_BUTTON_PIN 10
// Example: Read button to trigger camera
#define CAMERA_BUTTON_PIN 5

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  pinMode(SPEAKER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CAMERA_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  while (!Serial); // for SAMD
  delay(100);

  Serial.println("Ground Station Ping-Pong: GS Side");

  // Manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa init OK!");

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed!");
    while (1);
  }
  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);

  // Medium TX power
  rf95.setTxPower(13, false);

  // Start in receive mode
  rf95.setModeRx();
}

void loop() {
  // Listen for a telemetry packet from the rocket
  if (rf95.available()) {
    // 1) We got data
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      buf[len] = '\0';
      Serial.print("Got rocket telemetry: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);

      // 2) Decide what command to send back
      sendCommand();
    } else {
      Serial.println("Receive failed");
    }
  }
}

// Builds a command based on button states
void sendCommand() {
  // Example: If camera button pressed -> "CAMERA_ON"
  // If speaker button pressed -> "SPEAKER_ON", else "SPEAKER_OFF"

  String cmd;
  if (digitalRead(CAMERA_BUTTON_PIN) == LOW) {
    cmd = "CAMERA_ON";
  } else if (digitalRead(SPEAKER_BUTTON_PIN) == LOW) {
    cmd = "SPEAKER_ON";
  } else {
    cmd = "SPEAKER_OFF";
  }

  // Print to console
  Serial.print("Sending command: ");
  Serial.println(cmd);

  // Convert to c-string
  char commandBuf[20];
  cmd.toCharArray(commandBuf, sizeof(commandBuf));

  // Switch to TX
  rf95.setModeTx();
  rf95.send((uint8_t*)commandBuf, strlen(commandBuf));
  rf95.waitPacketSent();

  // Switch back to RX
  rf95.setModeRx();
}