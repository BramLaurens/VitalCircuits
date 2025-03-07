#include <Arduino.h>
#include <HardwareSerial.h>

#define RX_GPIO 26
#define TX_GPIO 27

int counter = 0;

HardwareSerial UARTtest(2);

void setup() {
  Serial.begin(9600);
  UARTtest.begin(9600, SERIAL_8N1, RX_GPIO, TX_GPIO);
  Serial.println("Setup!");

}

void loop() {

  while (UARTtest.available()) 
  {
    String RXdata = UARTtest.readString();
    Serial.print("Received: " + RXdata);
  }
  
}