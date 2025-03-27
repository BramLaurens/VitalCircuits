#include "Arduino.h"

const int sensorValues[] = {1200, 1800, 2200, 1900, 2300, 2100, 1700, 2200, 1800, 2400}; 
const int arraySize = sizeof(sensorValues) / sizeof(sensorValues[0]); 

int index = 0;           // Index voor array uitlezen
int counter = 0;         // Aantal hartslagen
unsigned long lastTime = 0;
unsigned long lastReadTime = 0;
const int measurementTime = 15000; // Meetperiode in milliseconden (15 sec)
const int readInterval = 1000; // Interval voor uitlezen van array (1 sec)
bool pulseDetected = false;
const int threshold = 2000; // Drempelwaarde voor hartslagdetectie

void setup() {
    Serial.begin(115200);
    lastTime = millis(); // Starttijd vastleggen
}

void loop() {
    if (millis() - lastReadTime >= readInterval) {
        lastReadTime = millis();

        int data = sensorValues[index];  // Lees een waarde uit de array
        Serial.print("Sensorwaarde: ");
        Serial.println(data);

        // Detecteer piek (hartslag)
        if (data >= threshold && !pulseDetected) {
            counter++;
            pulseDetected = true;
        }

        if (data < threshold) {
            pulseDetected = false;
        }

        index++; // Ga naar de volgende waarde
        if (index >= arraySize) {
            index = 0; // Reset naar begin van array
        }
    }

    // Bereken BPM elke 15 seconden
    if (millis() - lastTime >= measurementTime) {
        int bpm = (counter * (60000 / measurementTime)); // Dynamisch berekend
        Serial.print("Hartslag: ");
        Serial.print(bpm);
        Serial.println(" BPM");

        counter = 0; // Reset teller
        lastTime = millis(); // Reset tijd
    }
}