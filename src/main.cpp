#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h>

RH_ASK driver; // Create an instance of the RH_ASK driver for communication

// Define a structure to store sensor data
struct sensorData {
    unsigned char pressure_sensor;        // Pressure sensor data (0 - 255)
    int temperature_sensor;                // Temperature sensor data (-32,768 to 32,767)
    short unsigned int moisture_sensor;    // Moisture sensor data (0 - 65,535)
    unsigned char heartbeat[10];           // Heartbeat data (0 - 255) in an array of size 10
};

void setup() {
    Serial.begin(9600);  // Initialize serial communication for debugging at 9600 baud rate
    if (!driver.init()) {
        Serial.println("Initialization failed");  // If the driver initialization fails, print an error message
    }
}

void loop() {
    uint8_t buf[sizeof(sensorData)];  // Define a buffer to store the received data
    uint8_t buflen = sizeof(buf);     // Store the length of the buffer

    if (driver.recv(buf, &buflen)) {  // Check if data is received from the transmitter
        sensorData sensor_data_received;  // Create an instance to store the received sensor data

        memcpy(&sensor_data_received, buf, sizeof(sensor_data_received));  // Copy received bytes into the structure

        Serial.print("Received value: ");
        Serial.println(sensor_data_received.heartbeat[5]);  // Print the value of heartbeat index 5
    }
}
