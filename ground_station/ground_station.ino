#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS  8
#define RFM95_RST 4
#define RFM95_INT 3

#define RF95_FREQ 433.0

// Define button pins for command decision
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

  // Manual reset of LoRa module
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

  // Medium TX power is fine for ground station
  rf95.setTxPower(13, false);

  // Start in receiver mode
  rf95.setModeRx();

  Serial.println("Ground station ready!");
}

void loop() {
  if (rf95.available()) {
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len)) {
      buf[len] = '\0';
      String message = String((char*)buf);
      message.trim();
      
      // If the message is the special command request from the rocket...
      if (message == "CMD_REQ") {
        Serial.println("Received CMD_REQ from rocket.");
        // Give rocket time to switch to RX mode
        delay(50);
        String cmd = decideCommand();
        sendReply(cmd);
      } else {
        // Otherwise, treat it as telemetry data and display/forward it.
        Serial.print("Telemetry: ");
        Serial.println(message);
      }
    } else {
      Serial.println("Receive failed.");
    }
  }
  // Ensure we remain in RX mode
  rf95.setModeRx();
}

// Decide the command based on button states
String decideCommand() {
  // If the camera button is pressed, send "CAMERA_ON"
  if (digitalRead(CAMERA_BUTTON_PIN) == LOW) {
    return "CAMERA_ON";
  }
  // If the speaker button is pressed, send "SPEAKER_ON"
  if (digitalRead(SPEAKER_BUTTON_PIN) == LOW) {
    return "SPEAKER_ON";
  }
  // Otherwise, default to "SPEAKER_OFF"
  return "SPEAKER_OFF";
}

// Sends the command reply with a "CMD:" prefix to clearly mark it as a ground command.
void sendReply(const String& cmd) {
  String commandPacket = "CMD:" + cmd;
  char cmdBuf[30];
  commandPacket.toCharArray(cmdBuf, sizeof(cmdBuf));

  Serial.print("Sending command: ");
  Serial.println(commandPacket);

  rf95.setModeTx();
  rf95.send((uint8_t*)cmdBuf, strlen(cmdBuf));
  rf95.waitPacketSent();
  delay(10);
  rf95.setModeRx();
}
