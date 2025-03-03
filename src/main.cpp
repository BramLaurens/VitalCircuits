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
  while (Serial3.available()>0) 
  {
    String RXdata = Serial3.readStringUntil('\n');
    Serial.print("Received: " + RXdata);
  }
  
}