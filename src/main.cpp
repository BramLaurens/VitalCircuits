#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile

// Rx 21 
// Tx 19
RH_ASK driver(2000,21, 19);

struct sensordata_struct
{
    unsigned char pressure_sensor;        // 0 - 255
    int temperature_sensor;               // -32,768 - 32,767
    short unsigned int moisture_sensor;   // 0 - 65,535
    unsigned char heartbeat[10];          // 0 - 255
};

struct message_struct
{
  char soc;
  char pl;
  char src_ID;
  char des_ID;
  char pc;
  char functiecode;
  sensordata_struct data;
  char eot;
  char lrc;   
};

void setup() 
{
  Serial.begin(9600);

  if (!driver.init()) 
  {
    Serial.println("Initialization failed");  // If the driver initialization fails, print an error message
  }
}


void loop() 
{
  uint8_t buf[sizeof(message_struct)];  // Define a buffer to store the received data
  uint8_t buflen = sizeof(buf);     // Store the length of the buffer

  if (driver.recv(buf, &buflen)) 
  {  // Check if data is received from the transmitter
      message_struct message_recieved;  // Create an instance to store the received sensor data

      memcpy(&message_recieved, buf, sizeof(message_recieved));  // Copy received bytes into the structure

      // Print message info.
      Serial.print("functiecode: ");
      Serial.println(message_recieved.functiecode, HEX);
      Serial.print("src_ID: ");
      Serial.println(message_recieved.src_ID, HEX);
      Serial.print("des_ID: ");
      Serial.println(message_recieved.des_ID, HEX);
      Serial.print("pc: ");
      Serial.println(message_recieved.pc, HEX);
      Serial.print("pl: ");
      Serial.println(message_recieved.pl, HEX);

      // Print some data.
      Serial.println("Data:");
      Serial.print("Heartbeat[5]: ");
      Serial.println(message_recieved.data.heartbeat[5], HEX);  
      Serial.print("moisture sensor: ");
      Serial.println(message_recieved.data.moisture_sensor, HEX);
  }
}
