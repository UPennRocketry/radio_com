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
  // Generate telemetry data
  float velocity     = amplitude * sin(angle);
  float altitude     = amplitude * sin(angle + 1.0);
  float temperature  = amplitude * sin(angle + 2.0);
  float pressure     = amplitude * sin(angle + 3.0);
  angle += angleIncrement;

  // Format the telemetry packet
  char packet[100];
  snprintf(packet, sizeof(packet),
           "Velocity:%.2f,Altitude:%.2f,Temperature:%.2f,Pressure:%.2f",
           velocity, altitude, temperature, pressure);

  Serial.print("Sending telemetry: ");
  Serial.println(packet);

  rf95.setModeTx();
  rf95.send((uint8_t*)packet, strlen(packet));
  rf95.waitPacketSent();
  // Remain in TX mode during telemetry period
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
