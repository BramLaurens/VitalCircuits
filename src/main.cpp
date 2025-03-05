#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h>

RH_ASK driver;

// Define the structure to hold sensor data
struct sensorData 
{
    uint8_t pressure_sensor;        // Pressure sensor value (0 - 255)
    int32_t temperature_sensor;     // Temperature sensor value (-32,768 to 32,767)
    uint16_t moisture_sensor;       // Moisture sensor value (0 - 65,535)
    uint8_t heartbeat[10];          // Array for heartbeat data (0 - 255)
};

sensorData data;  // Create an instance of sensorData structure

void setup() 
{
    Serial.begin(9600);  // Initialize serial communication for debugging
    if (!driver.init()) 
    {
        Serial.println("Initialization failed"); // Print error message if initialization fails
    }
    
    data.pressure_sensor = 120;  // Example pressure sensor value
    data.temperature_sensor = -25;  // Example temperature value
    data.moisture_sensor = 512;  // Example moisture sensor value
    
    // Fill heartbeat array with example values (0, 1, 2, ..., 9)
    for (int i = 0; i < 10; i++) 
    {
        data.heartbeat[i] = i;
    }
}

void loop() 
{

    unsigned long start = millis(); // Save the current time (miliseconds since program start)

    // Send the sensor data as a byte array
    driver.send((uint8_t*)&data, sizeof(data));
    driver.waitPacketSent(); // Wait until data is fully sent


    // Data send and output the duration of the package send.
    Serial.print("Data send. duration: ");
    Serial.println(millis() - start);
    
    delay(10);
}
