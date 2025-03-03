#include <Arduino.h>
#include <HardwareSerial.h>

#define RX_GPIO 26
#define TX_GPIO 27

int counter = 0;

HardwareSerial Serial3(2);

void setup() {
  Serial.begin(115200);
  Serial3.begin(115200, SERIAL_8N1, RX_GPIO, TX_GPIO);

}

void loop() {
  Serial3.println(String(counter));

  counter++;
  delay(1000);
}