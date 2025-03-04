#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile

RH_ASK driver;

struct sensorData {
    unsigned char pressure_sensor;        // 0 - 255
    int temperature_sensor;                // -32,768 - 32,767
    short unsigned int moisture_sensor;       // 0 - 65,535
    unsigned char heartbeat[10];             // 0 - 255
};


struct protocol{
  char soc;
  char pl;
  char src_ID;
  char des_ID
  char pc;
  char functiecode;
  sensorData data;
  char eot;
  char lrc;   
}





void setup() {
    Serial.begin(9600);
    if (!driver.init()) {
        Serial.println("init failed");
    }
      // Create a test struct with values from 0 - 100
  testData = {
    11,  // pressure_sensor
    50,                      // temperature_sensor
    100,                                      // moisture_sensor
    {0}                                       // heartbear (initialize all to 0)
  };
    // Optionally, fill heartbear with values from 0 to 99
  for (int i = 0; i < 10; i++) {
    testData.heartbeat[i]=i;
  }

protocol eind_structuur
eind_structuur.soc = 1;
eind_structuur.pl = sizeof(eind_structuur);
eind_structuur.src_ID = 0000;
eind_structuur.des_ID = 0100;
eind_structuur.pc= 3;
eind_structuur.functiecode= 2;
eind_structuur.data= testData;
eind_structuur.eot =2;
eind_structuur.lrc = 00;



  /*
  protocool ={
    soc = 1,
    sizeoff,
    00000100,
    00000100,
    pakket nummer,
    functie code,
    data,
    2,
    foutcontrole,
  }
  */

  }
}



void loop() {
 
    //int number[10] = {1, 252, 3, 4, 5, 6, 7, 8, 9, 10}; // Getal dat je wilt verzenden

    driver.send((uint8_t *)&testData, sizeof(testData)); // Verstuur als bytes
    driver.waitPacketSent();

    Serial.print("Struct verzonden!");
    

    delay(10);
}
