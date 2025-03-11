/*
* Arduino Wireless Communication Tutorial
*     Example 1 - Transmitter Code
*                
* by Dejan Nedelkovski, www.HowToMechatronics.com
* 
* Library: TMRh20/RF24, https://github.com/tmrh20/RF24/
*/

#include <SPI.h>
#include <RF24.h>

RF24 radio(4, 5); // CE, CSN

const byte address[6] = "00001";


// Define the struct for the message
struct sensordata_struct
{
    unsigned char pressure_sensor[60];        // 0 - 255
    int temperature_sensor;               // -32,768 - 32,767
    short unsigned int moisture_sensor;   // 0 - 65,535
    short int heartbeat[60];                // -32,768 - 32,767
};

//the total size of the struct is 192 bytes
// explanation
// 60 bytes for pressure_sensor
// 2 bytes for temperature_sensor
// 2 bytes for moisture_sensor
// 120 bytes for heartbeat
// 8 bytes for the rest of the struct


struct message_struct
{
  char soc;
  char pl;
  char src_ID;
  char des_ID;
  char pc;
  char functiecode;
  sensordata_struct data;
  char lrc;   
  char eot;
};

sensordata_struct test_data;
message_struct message; 



void setup() {
	Serial.begin(9600);
	radio.begin();
	radio.openWritingPipe(address);
	radio.setPALevel(RF24_PA_HIGH);
	radio.setDataRate(RF24_2MBPS);
	radio.setAutoAck(false);
	radio.stopListening();
	radio.enableDynamicPayloads();
	Serial.println("Setup");

	// fill test_data
	for (int i = 0; i < 60; i++)
	{
		test_data.pressure_sensor[i] = i;
		test_data.heartbeat[i] = i;
	}
	test_data.temperature_sensor = 12345;
	test_data.moisture_sensor = 54321;

	// fill message
	message.soc = 0x01;
	message.pl = 0x01;
	message.src_ID = 0x01;
	message.des_ID = 0x07;
	message.pc = 0x01;
	message.functiecode = 0x01;
	message.data = test_data;
	message.eot = 0x04;
	message.lrc = 0x01;



}

void loop() {
  String text = "Hello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello WorldHello World";
  Serial.println(radio.write(&text, sizeof(text)));
  
  //Serial.print("Data Sent! (");
  //Serial.print(text);
  //Serial.println(")");
  delay(100);
}