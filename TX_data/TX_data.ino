#include <SPI.h>
#include <RH_RF95.h>
#include <math.h>

#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 3
#define RF95_FREQ 433.0

RH_RF95 rf95(RFM95_CS, RFM95_INT);

void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  Serial.begin(9600);
  delay(100);

  Serial.println("Arduino LoRa TX - Sine Wave Stream with Custom Format");

  // Manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);

  // Lower power if modules are close
  rf95.setTxPower(13, false);
}

void loop() {
  static float angle = 0.0; // Angle for sine wave
  const float angleIncrement = 0.1; // Increment angle by 0.1 radians each step
  const float amplitude = 100.0; // Amplitude of the sine wave

  // Generate sine wave values for each parameter
  float velocity = amplitude * sin(angle);
  float altitude = amplitude * sin(angle + 1.0); // Phase shift for variety
  float temperature = amplitude * sin(angle + 2.0);
  float pressure = amplitude * sin(angle + 3.0);

  angle += angleIncrement; // Increment angle

  // Format the data into a string
  char radiopacket[100];
  snprintf(radiopacket, sizeof(radiopacket), "Velocity:%.2f,Altitude:%.2f,Temperature:%.2f,Pressure:%.2f", velocity, altitude, temperature, pressure);

  Serial.print("Sending: ");
  Serial.println(radiopacket);

  // Send the formatted string
  rf95.send((uint8_t *)radiopacket, strlen(radiopacket));
  rf95.waitPacketSent();

  // Wait for a short period before sending the next value
  delay(100); // Adjust delay to control the transmission rate
}