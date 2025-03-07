#include <Arduino.h>
#include <HardwareSerial.h>

#define RX_GPIO 26
#define TX_GPIO 27

int counter = 0;

HardwareSerial Serial3(2);

void setup() {
  Serial.begin(9600);
  Serial3.begin(9600, SERIAL_8N1, RX_GPIO, TX_GPIO);
  Serial.println("Setup!");

}

void loop() {

  while (Serial3.available()>0) 
  {
    String RXdata = Serial3.readString();
    Serial.print("Received: " + RXdata);
  }
  
}