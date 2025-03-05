#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile

RH_ASK driver;


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

sensordata_struct test_data;
message_struct message;


void setup() 
{
  Serial.begin(9600);

  if (!driver.init()) 
  {
    Serial.println("init failed");
  }

  // Create test data
  // Evantually this will be replaced by real sensor data.
  test_data = {
    11,       // pressure_sensor
    50,       // temperature_sensor
    100,      // moisture_sensor
    {0}       // heartbear (initialize all to 0)
  };

  // Fill heartbear with values from 0 to 99
  for (int i = 0; i < 10; i++) {
    test_data.heartbeat[i]=i;
  }

  // Create the final message.
  message.soc = 1;                                    // Start of communication
  message.src_ID = 0x04;                              // (0x04 = 0000 0100) The source ID for EV1A Group 4.
  message.des_ID = 0x04;                              // The destination ID, in this case EV1A Group 4.
  message.pc= 1;                                      // The packet counter, in this case 1.
  message.functiecode = 2;                            // The function code, in this case 2 (Data verzenden).
  message.data = test_data;                           // The data
  message.eot = 0xFF;                                 // End of transmission bit. This is FF in our case.
  message.lrc = 0;                                    // TODO: The Longitudinal Redundancy Check
  
  message.pl = sizeof(message) - sizeof(message.lrc); // The packet length is the size of the total message minus the lrc part.
}


void loop() 
{
    unsigned long start = millis(); // Save the current time (miliseconds since program start)

    driver.send((uint8_t *)&message, sizeof(message)); 
    driver.waitPacketSent();


    // Data send and output the duration of the package send.
    Serial.print("Data send. duration: ");
    Serial.println(millis() - start);
    
    delay(10);
}
