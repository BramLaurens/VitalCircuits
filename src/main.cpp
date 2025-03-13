#include <Arduino.h>

TaskHandle_t data_versturen;

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
    char lrc;              // Longitudinal redundancy check
    char eot;              // End of transmission
};

message_struct message = {0, 0, 0, 0}; // Not sure if this is permitted
sensordata_struct live_data; 
int counter = 1;  // Packet counter
bool send = false;



void setup() {
    Serial.begin(9600);
    pinMode(35, INPUT);  // Pressure sensor
    pinMode(34, INPUT);  // Heartbeat sensor
    pinMode(33, INPUT);  // Moisture sensor
    pinMode(32, INPUT);  // Temperature sensor

    // Initialize message structure
    message.soc = 1;
    message.src_ID = 0x04;
    message.des_ID = 0x04;
    message.pc = counter;
    message.functiecode = 2;
    message.eot = 0xFF;
    message.lrc = 0;

    // Initialize RF driver
    if (!driver.init()) {
        Serial.println("RF driver initialization failed!");
        while (1);
    }

    xTaskCreatePinnedToCore(
        Task1code, 
        "data_versturen",
        10000,
        NULL,
        1,
        &data_versturen,
        0
    );  
}

void loop() {

     // Collect data only if not sending
        for (int a = 0; a < 55; a++) {
            live_data.heartbeat_sensor[a] = analogRead(34);
            live_data.pressure_sensor[a] = analogRead(35);
            delay(2);
        }

        live_data.moisture_sensor = analogRead(33);
        live_data.temperature_sensor = analogRead(32);
        while (send) {
        // Update message
        message.data = live_data;
        send = true; 
        

        while (!klaar_voor_nieuwe_data) // Wachten 

        message.data = live_data
        klaar_om_te_sturen = true;


        // Debug output
        /*
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

        */
        }    
}

void data_versturen(void *pvParameters) {
    for (;;) { // Infinite loop
        // Lora verzend code.
        while (1);
    }
}
