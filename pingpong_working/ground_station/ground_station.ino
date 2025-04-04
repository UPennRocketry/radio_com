// GroundStationPong.ino
#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS  8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 433.0

// Example: Use button pins to decide command
#define SPEAKER_BUTTON_PIN 10
#define CAMERA_BUTTON_PIN 5

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  pinMode(SPEAKER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CAMERA_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  while (!Serial);
  delay(100);

  Serial.println("Ground Station: Ping-Pong - GS Side");

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

  // Medium TX power is fine
  rf95.setTxPower(13, false);

  // Always listening until rocket pings
  rf95.setModeRx();

  Serial.println("Ground station ready!");
}

void loop() {
  // Listen for rocket telemetry
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      buf[len] = '\0';
      Serial.println((char*)buf);

      // Decide command to send
      String cmd = decideCommand();
      sendReply(cmd);
    } else {
      Serial.println("Receive failed");
    }
  }
}

/**
 * Picks a command based on button states
 */
String decideCommand() {
  // If camera button pressed => "CAMERA_ON"
  if (digitalRead(CAMERA_BUTTON_PIN) == LOW) {
    return "CAMERA_ON";
  }

  // If speaker button pressed => "SPEAKER_ON"
  if (digitalRead(SPEAKER_BUTTON_PIN) == LOW) {
    return "SPEAKER_ON";
  }

  // Otherwise => "SPEAKER_OFF"
  return "SPEAKER_OFF";
}

/**
 * Sends a single command back to the rocket
 */
void sendReply(const String& cmd) {
  char cmdBuf[20];
  cmd.toCharArray(cmdBuf, sizeof(cmdBuf));

  Serial.print("Sending command: ");
  Serial.println(cmd);

  rf95.setModeTx();
  rf95.send((uint8_t*)cmdBuf, strlen(cmdBuf));
  rf95.waitPacketSent();

  // Small delay to let final bits finish
  delay(10);

  // Switch back to RX listening for next rocket ping
  rf95.setModeRx();
}