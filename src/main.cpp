#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile

RH_ASK driver(3000);

struct sensordata_struct {
    unsigned char pressure_sensor[55];    // 0 - 255
    int temperature_sensor;               // -32,768 - 32,767
    short unsigned int moisture_sensor;   // 0 - 65,535
    short int heartbeat_sensor[55];       // 0 - 255
};

struct message_struct {
    char soc;              // Start of communication
    char pl;               // Packet length
    char src_ID;           // Source ID
    char des_ID;           // Destination ID
    char pc;               // Packet counter
    char functiecode;      // Function code
    sensordata_struct data;  // Sensor data
    char eot;              // End of transmission
    char lrc;              // Longitudinal redundancy check
};

message_struct message;
sensordata_struct live_data; 
int heartbeatIndex = 0;       // Index for heartbeat array
int pressureIndex = 0;        // Index for pressure array
int counter = 1;              // Packet counter

void setup() {
    Serial.begin(9600);
    pinMode(15, INPUT);  // Pressure sensor
    pinMode(16, INPUT);  // Heartbeat sensor
    pinMode(17, INPUT);  // Moisture sensor
    pinMode(18, INPUT);  // Temperature sensor

    // Initialize the message structure
    message.soc = 1;                         // Start of communication
    message.src_ID = 0x04;                   // Source ID for EV1A Group 4
    message.des_ID = 0x04;                   // Destination ID for EV1A Group 4
    message.pc = counter;                    // Packet counter
    message.functiecode = 2;                 // Function code (2 = Data transmit)
    message.eot = 0xFF;                      // End of transmission
    message.lrc = 0;                         // LRC (Longitudinal Redundancy Check)
}

void loop() {
    // Read sensor values
    int pressure_value = analogRead(15);
    int heartbeat_value = analogRead(16);
    live_data.moisture_sensor = analogRead(17);
    live_data.temperature_sensor = analogRead(18);

    // Add sensor values to arrays (circular buffer)
    live_data.heartbeat_sensor[heartbeatIndex] = heartbeat_value;
    heartbeatIndex = (heartbeatIndex + 1) % 55;  

    live_data.pressure_sensor[pressureIndex] = pressure_value;
    pressureIndex = (pressureIndex + 1) % 55;  

    // Update message data with the latest sensor readings
    message.data = live_data;

    // Print data to the serial monitor
    Serial.print("Druksensor waarden: ");
    for (int b = 0; b < 55; b++) {
        Serial.print(live_data.pressure_sensor[b]);
        Serial.print(" ");
    }
    Serial.println();
    
    Serial.print("Temperatuur: ");
    Serial.println(live_data.temperature_sensor);

    Serial.print("Vochtigheidssensor: ");
    Serial.println(live_data.moisture_sensor);

    Serial.print("Heartbeat waarden: ");
    for (int i = 0; i < 55; i++) {
        Serial.print(live_data.heartbeat_sensor[i]);
        Serial.print(" ");
    }
    Serial.println();

    // Send message when buffer is full (heartbeatIndex == 0)
    if (heartbeatIndex == 0) {
        // Assuming the driver is properly configured for communication
        driver.send((uint8_t *)&message, sizeof(message)); 
        driver.waitPacketSent(); // Wait until the packet is sent
        counter++;               // Increment packet counter
        message.pc = counter;    // Update packet counter in the message
    }

    // Optionally clear arrays if you want to reset the data
    if (heartbeatIndex == 0) {
        memset(live_data.heartbeat_sensor, 0, sizeof(live_data.heartbeat_sensor)); 
        memset(live_data.pressure_sensor, 0, sizeof(live_data.pressure_sensor)); 
    }
   delay(2);
}
