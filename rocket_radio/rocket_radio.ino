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
#define RRC3_TX 1  // TX to RRC3 Rx
#define RRC3_RX 0  // RX from RRC3 Tx

RH_RF95 rf95(RFM95_CS, RFM95_INT);

// Telemetry variables
static float angle = 0.0;
const float angleIncrement = 0.1;
const float amplitude      = 100.0;

// Timing parameters (in milliseconds)
const unsigned long telemetryPeriod = 2000; // 2 seconds of telemetry transmission
const unsigned long commandWindow   = 500;  // 0.5 second window to wait for command

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
  Serial1.begin(9600); // Initialize RRC3_RX serial communication
  while (!Serial); // for SAMD
  delay(100);

  Serial.println("Rocket Ping-Pong: Rocket Side");

  // Manual reset of LoRa module
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
  
  // Start in TX mode
  rf95.setModeTx();

  Serial.println("Rocket ready to ping...");
}

void loop() {
  unsigned long cycleStart = millis();
  
  // Transmit telemetry continuously for telemetryPeriod
  while (millis() - cycleStart < telemetryPeriod) {
    sendTelemetry();
    delay(100); // Adjust telemetry rate as needed
  }
  
  // Flush any lingering data in RX FIFO (if any)
  flushRadio();

  // Send the special command request
  sendSpecialRequest();
  
  // Short delay to ensure the transmission is fully out
  delay(50);
  
  // Switch to RX mode to wait for ground command reply
  rf95.setModeRx();
  unsigned long cmdStart = millis();
  bool properCommandReceived = false;
  
  while (millis() - cmdStart < commandWindow) {
    if (rf95.available()) {
      if (handleGroundCommand()) {
        properCommandReceived = true;
        break;
      }
    }
  }
  
  if (!properCommandReceived) {
    Serial.println("No valid command received in window.");
  }
  
  // Switch back to TX mode for next cycle
  rf95.setModeTx();
}

// Sends a telemetry packet in TX mode
void sendTelemetry() {
  // Buffer to store incoming telemetry data from RRC3
  char telemetryBuffer[100];
  int index = 0;

  // Timeout for waiting on telemetry data (in milliseconds)
  const unsigned long telemetryTimeout = 1500;
  unsigned long startTime = millis();

  // Read data from RRC3_RX until a carriage return ('\r') is received or timeout occurs
  while (millis() - startTime < telemetryTimeout) {
    if (Serial1.available() > 0) {
      char c = Serial1.read();
      if (c == '\r') {
        telemetryBuffer[index] = '\0';
        break;
      }
      if (index < sizeof(telemetryBuffer) - 1) {
        telemetryBuffer[index++] = c;
      }
    }
  }

  // If no valid telemetry data was received, skip sending
  if (index == 0) {
    Serial.println("No telemetry data received from RRC3 within timeout.");
    return;
  }

  // RRC3 should follow telemetry data format from user manual pages 16-17
  char* token = strtok(telemetryBuffer, ",");
  float timestamp = 0;
  float altitude = 0;
  float velocity = 0;
  float temperature = 0;

  if (token != NULL) {
    timestamp = atof(token);
  }
  token = strtok(NULL, ",");
  if (token != NULL) {
    altitude = atof(token);
  }
  token = strtok(NULL, ",");
  if (token != NULL) {
    velocity = atof(token);
  }
  token = strtok(NULL, ",");
  if (token != NULL) {
    temperature = atof(token);
  }

  // Format the telemetry packet to send by radio
  char packet[100];
  snprintf(packet, sizeof(packet),
           "Timestamp:%.1f,Altitude:%.2f,Velocity:%.2f,Temperature:%.2f",
           timestamp, altitude, velocity, temperature);

  Serial.print("Sending telemetry: ");
  Serial.println(packet);

  // Send the reformatted telemetry packet by radio
  rf95.setModeTx();
  rf95.send((uint8_t*)packet, strlen(packet));
  rf95.waitPacketSent();
}

// Flush any pending data from the radio FIFO
void flushRadio() {
  while (rf95.available()) {
    uint8_t dummyBuf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t dummyLen = sizeof(dummyBuf);
    rf95.recv(dummyBuf, &dummyLen);
  }
}

// Sends the special command request message ("CMD_REQ")
void sendSpecialRequest() {
  const char specialMsg[] = "CMD_REQ";
  Serial.println("Sending special command request.");
  
  rf95.setModeTx();
  rf95.send((uint8_t*)specialMsg, strlen(specialMsg));
  rf95.waitPacketSent();
  // Do not switch to RX here; handled in main loop.
}

// Returns true if a valid ground command was processed.
bool handleGroundCommand() {
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  
  if (rf95.recv(buf, &len)) {
    buf[len] = '\0'; // Null-terminate the received message
    String receivedStr = String((char*)buf);
    receivedStr.trim();
    
    Serial.print("Received command: ");
    Serial.println(receivedStr);

    // Check if the packet is a ground command (should start with "CMD:")
    if (receivedStr.startsWith("CMD:")) {
      // Remove the "CMD:" prefix before processing
      String command = receivedStr.substring(4);
      command.trim();
      
      // Act on the ground command
      if (command == "SPEAKER_ON") {
        Serial.println("Activating Speaker.");
        digitalWrite(SPEAKER_PIN, LOW);  // Active low
      } else if (command == "SPEAKER_OFF") {
        Serial.println("Deactivating Speaker.");
        digitalWrite(SPEAKER_PIN, HIGH);
      } else if (command == "CAMERA_ON") {
        Serial.println("Activating Camera.");
        digitalWrite(CAMERA_PIN, HIGH);
        delay(500);
        digitalWrite(CAMERA_PIN, LOW);
      } else {
        Serial.println("Unknown command received.");
      }

      // Optional: Blink LED to signal command processing
      digitalWrite(LED, HIGH);
      delay(100);
      digitalWrite(LED, LOW);
      
      return true; // Valid ground command processed
    }
    else {
      Serial.println("Received packet is not a valid ground command. Ignoring.");
    }
  }
  else {
    Serial.println("Receive failed on command packet.");
  }
  return false; // No valid command was processed
}
