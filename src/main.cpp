#define LoRa_E220_DEBUG
#define FREQUENCY_868
#define MESSAGE_TIMEOUT 1000

#define ECG_PIN 34
#define PRESSURE_PIN 35
#define MOISTURE_PIN 33
#define TEMPERATURE_PIN 32

// COMMENT OR UNCOMMENT DEBUG STATEMENTS FOR DEBUGGING OPTIONS
	// #define comms_debug
	// #define temp_debug
// END OF DEBUG STATEMENTS

#include "Arduino.h"
#include "LoRa_E220.h"
#include <math.h>

TaskHandle_t TXRX_task;

// ---------- esp32 pins --------------
LoRa_E220 e220ttl(&Serial2, 15, 21, 19); // (RX TX) AUX M0 M1


// Define the struct for the sensordata
struct sensordata_struct
{
    unsigned char pressure_sensor[59];        // 0 - 255
    int temperature_sensor;                   // -32,768 - 32,767
    short unsigned int moisture_sensor;   	  // 0 - 65,535
    short int heartbeat[59];				  // -32,768 - 32,767
};

// Define the struct for the output message
struct message_struct
{
	char soc;
	char pl;
	char src_ID;
	char des_ID;
	unsigned char p_ID;
	char functiecode;
	sensordata_struct data;
	int lrc;   
char eot;
};

// Define the struct for the input message
struct response_message_struct
{
	char soc;
  	char pl;
  	char src_ID;
  	char des_ID;
  	unsigned char p_ID;
  	char functiecode;
	char ack_ID;
 	int lrc;   
  	char eot;
};

// Initialize variables
sensordata_struct sensordata_buffer[256];
char timing_ID = 0;
char message_times[] = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
unsigned long time_last_message_send;
int datasample_interval;
bool lastmessage_done = false;
bool data_pulled = true; //Init with true so that first sample can be taken and newdata_avail becomes true
bool newdata_available = false;

// Protoypes
void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);
void SetLoRaConfig(); // Unused
void send_message(message_struct message);
bool ReceiveLoRa();
void getLoRaConfig();
void lora_TXRX(void *pvParameters);
double temp_calc();

int create_LRC(message_struct message);
int create_LRC_ack(response_message_struct message);
int count_bits(int num);

int rolling_averagetime();


// Create a sensordata struct with testdata, this will later be filled with real sensor data
sensordata_struct test_data;

// Create a sensordata struct to store the live data
sensordata_struct live_data;

// Create a message struct to store the message to be sent
message_struct message;

// Create a message struct to store the received message
response_message_struct recieved_message;

void setup()
{
	// Begin serial ports
	Serial.begin(115200); 									// For debugging (Serial monitor)
	Serial2.begin(9600, SERIAL_8N1, 16, 17);  				// For communication to the LoRa module
	delay(500);												// Give the Serial some time to initialize

	pinMode(PRESSURE_PIN, INPUT);  // Pressure sensor
    pinMode(ECG_PIN, INPUT);  // Heartbeat sensor
    pinMode(MOISTURE_PIN, INPUT);  // Moisture sensor
    pinMode(TEMPERATURE_PIN, INPUT);  // Temperature sensor

	// -- THE FOLLOWING CODE IS FOR TESTING PURPOSES ONLY --
	// fill test_data
	for (int i = 0; i < 59; i++)
	{
		test_data.pressure_sensor[i] = i;
		test_data.heartbeat[i] = i;
	}
	test_data.temperature_sensor = 12345;
	test_data.moisture_sensor = 54321;
	// -- THE ABOVE CODE IS FOR TESTING PURPOSES ONLY --


	// Fill protocol
	message.soc = 0x7E; 									// Self chosen value
	message.src_ID = 0x04;									// Source ID - 0000 0001 In our case (EV1A Group 4)
	message.des_ID = 0x04;									// Destination ID - 0000 0111 In our case (EV1A Group 4)
	message.p_ID = 0x00;									// Package ID - Starts at 0
	message.eot = 0xA4; 									// Self chosen value
	message.pl = sizeof(message) - sizeof(message.lrc);		// Payload length - 1 byte
	

	// Begin communication to the LoRa module
	// Startup all pins and UART
	e220ttl.begin();

	// Set e220 to normal mode
	e220ttl.setMode(MODE_0_NORMAL);


	ResponseStructContainer c;
	c = e220ttl.getConfiguration();
	// It's important get configuration pointer before all other operation
	Configuration configuration = *(Configuration *)c.data;

	// Get configuration
	getLoRaConfig();

	// Set configuration
	// SetLoRaConfig();

	//Create a task to send data and pin it to core 0
	xTaskCreatePinnedToCore(
        lora_TXRX, 
        "TXRX_task",
        10000,
        NULL,
        1,
        &TXRX_task,
        0
    );  

	// set new serial speed (e220 needs 9600 to config and sends data on 115200)
	Serial2.flush();
	Serial2.end();
	Serial2.begin(115200);
}

// TXRX loop, this loop is used to send and recieve messages from the lora module and runs on a different core
void lora_TXRX(void *pvParameters) 
{
    for (;;) 
	{ // Infinite loop
		// If there is new data available and the last message is sent and acknowledged, send the new data
		if(newdata_available && lastmessage_done){

			// Reset variables, stop task 1 from putting new data in the live buffer
			lastmessage_done = false;
			message.p_ID++; // NOTE: we start with packet 1. This makes resending packets easier.

			// Update data
			message.data = live_data;
			data_pulled = true;

			// Set functiecode naar 0x02 (send data)
			message.functiecode = 0x02;

			// Send message
			send_message(message);
		
		}

		// Wait for response
		if (ReceiveLoRa()) 
		{
			// A message was recieved
			Serial.println(" ");
			
			// Check if the message is an acknowledge or a retransmit
			// This if statement also checks the ack LRC, if the ack LRC isn't correct, the message is ignored.
			if (recieved_message.functiecode == 0x05 && recieved_message.lrc == create_LRC_ack(recieved_message))
			{
				// recieved ackowledge - sending next package
				// Debug message
				Serial.println(" ");
				Serial.println("RECIEVED ACKNOWLEDGE");
				Serial.print("Recieved ackowledge for package: ");
				Serial.print(recieved_message.ack_ID, DEC);
				Serial.print(" - ack_ID: ");
				Serial.print(recieved_message.ack_ID, DEC);
				Serial.print(" - t: (");
				Serial.print(millis());
				Serial.println(")");
				Serial.println(" ");                 
				// END Debug message

				//Mark verification routine as done
				lastmessage_done = true;
				

			}

			// if recieved retransmit - sending the requested package
			else if (recieved_message.functiecode == 0x01 && recieved_message.lrc == create_LRC_ack(recieved_message))
			{
				// Debug message
				Serial.print("Recieved retramsnit request for package: ");
				Serial.print(recieved_message.ack_ID, DEC);
				Serial.print(" - t: (");
				Serial.print(millis());
				Serial.println(")");
				// END Debug message

				
				// Set message data the data of the requested package
				message.data = sensordata_buffer[recieved_message.ack_ID];
				
				// Set functiecode naar 0x06 (retransmit)
				// NOTE - this functiecode is not in the original protocol
				message.functiecode = 0x06;		
				
				// Set the package ID to the requested package
				message.p_ID = recieved_message.ack_ID;
				
				// Generate LRC
				message.lrc = create_LRC(message);

				// Send message
				send_message(message);
			}

			else 
			{
				// Functiecode is not recognized or LRC is not correct
				// In this case we wait until we recieve a new message (acknowledge or retransmit)
				Serial.println("ERROR -  functiecode not recognized or LRC not correct.");
			}
		}

		// If there is no message available, check if the last message was send more than 85 ms ago
		else if (millis() - time_last_message_send > MESSAGE_TIMEOUT)
		{
			// Last message is more than 85 ms ago - resend the message

			#ifdef comms_debug
				Serial.print("WARNING - Got no acknowledge for message: ");
				Serial.print(message.p_ID, DEC);
				Serial.println(" - Resending message.");
			#endif

			// Resend message
			send_message(message);
		}
    }
}

// Main Loop, this loop is used to collect sensor data and process it.
void loop()
{
	// Collect and process sensor data
	// Calculate optimal sample interval for heartbeat sensor based on the rolling average latency of the last 10 messages
	datasample_interval = rolling_averagetime() / 59;

	// Wait for data to be pulled in task 0
	if(data_pulled)
	{
		//Mark that new data is not available right now so that TXRX wont send the same data again
		newdata_available = false;

		// Collect sensor data when last message is sent and acknowledged
		for(int i = 0; i < 59; i++)
		{
			live_data.heartbeat[i] = map(analogRead(25), 0, 4095, 0, 32767);
			live_data.pressure_sensor[i] = map(analogRead(35), 0, 4095, 0, 255);
			
			//Serial.println(map(analogRead(25), 0, 4095, 0, 32767));
			//Serial.println(live_data.heartbeat[i]);
			delay(datasample_interval);
			//Serial.println(datasample_interval);
		}

		live_data.moisture_sensor = analogRead(33);
		live_data.temperature_sensor = analogRead(32);

		// Flag new data as available and not pulled yet so that TXRX can send the data
		newdata_available = true;
		data_pulled = false;
	}



}

double temp_calc()
{
	float temp_raw = analogRead(TEMPERATURE_PIN);
	float v_diff = (temp_raw * 3.3 / 4095) / 3.704;
	float v_ntc = 1.57 - v_diff;
	float I_ntc = (3.31 - v_ntc) / 10000;
	float r_ntc = v_ntc / I_ntc;
	double lnR = log(r_ntc);

	float tempK = 1 / (2.386e-3 + 0.169e-4 * lnR + 10.14e-7 * pow(lnR, 3));
	double tempC = tempK - 273.15 + 4;

	#ifdef temp_debug
		Serial.print("temp_raw: ");
		Serial.print(temp_raw);
		Serial.print("	v_diff: ");
		Serial.print(v_diff);
		Serial.print("	v_ntc: ");
		Serial.print(v_ntc);
		Serial.print("	I_ntc: ");
		Serial.print(I_ntc);
		Serial.print("	r_ntc: ");
		Serial.print(r_ntc);
		Serial.print("	lnR: ");
		Serial.print(lnR);
		Serial.print("	tempK: ");
		Serial.print(tempK);
		Serial.print("	tempC: ");
		Serial.println(tempC);
	#endif

	delay(100);
	return tempC;
}

// A function that sends a message to the lora module.
void send_message(message_struct message) 
{
	// Save sensor data in buffer
	sensordata_buffer[message.p_ID] = message.data;
	
	// Create LRC
	message.lrc = create_LRC(message);
	
	// Debug message
	#ifdef comms_debug
		Serial.println("SENT MESSAGE");
		Serial.print("(t: ");
		int timer = millis();
		Serial.print(timer);
		Serial.print(") - Status: ");
	#endif
	// END Debug message


	// Send message
	// NOTE - This function blocks the program until there is a response
	ResponseStatus rs = e220ttl.sendMessage((uint8_t *)&message, sizeof(message));


	// Debug message
	#ifdef comms_debug
		Serial.print(rs.getResponseDescription());
		Serial.print(" (t: ");
		Serial.print(millis());
		Serial.print(") - Diff: (");
		Serial.print(millis() - timer);
		Serial.print(") ");
		Serial.print(" - p_ID: ");
		Serial.print(message.p_ID, DEC);
	#endif
	// END Debug message


	//Update the message times ID in the array
	message_times[timing_ID] = millis() - time_last_message_send;

	// Save the time the message was sent and increase the timing_ID
	time_last_message_send = millis();
	timing_ID++;

	//	Go back to the first position if the timing array is full
	if(timing_ID > 9)
	{
		timing_ID = 0;
	}

	#ifdef comms_debug
		Serial.print(" - Timing: ");
		// Print the timing array
		for(int i = 0; i < 10; i++)
		{
			Serial.print(message_times[i], DEC);
			Serial.print(" ");
		}
		Serial.print(" - Average: ");
		Serial.println(rolling_averagetime());
		Serial.println(" ");
	#endif
}

// A function that receives a message from the lora module.
// This function returns a boolean, true if a message was received, false if no message was received.
bool ReceiveLoRa(){
	// If there is a message avaiable
	if (e220ttl.available())
	{
		// create a ResponseStructContainer and fill it with the received message
		ResponseStructContainer rsc = e220ttl.receiveMessage(sizeof(response_message_struct));

		// TODO: Chek if 
		// The received message is extracted from the entire message and stored
		recieved_message = *(response_message_struct *)rsc.data;

		// Is something goes wrong print error
		if (rsc.status.code != 1)
		{
			Serial.print("Error with RSC: ");
			Serial.println(rsc.status.getResponseDescription());
		}

		// Close the ResponseStructContainer rsc
		rsc.close();

		return true;
	}

	// If there is no message avaiable
	else 
	{
		return false;
	}
}

// Set LoRa module configuration
// This function is not called, since the LoRa module is already configured and set to remember the configuration
void SetLoRaConfig()
{
	// Create a ResponseStructContainer and fill it with the configuration
	ResponseStructContainer c;
	c = e220ttl.getConfiguration();
	// It's important get configuration pointer before all other operation
	Configuration configuration = *(Configuration *)c.data;

	// Print configuration satus and parameters.
	Serial.println(c.status.getResponseDescription());
	Serial.println(c.status.code);
	printParameters(configuration);
	
	// Configuration
	configuration.ADDL = 0x03; // First part of address
	configuration.ADDH = 0x00; // Second part

	configuration.CHAN = 18; // Communication channel

	configuration.SPED.uartBaudRate = UART_BPS_115200;		// Serial baud rate
	configuration.SPED.airDataRate = AIR_DATA_RATE_111_625; // Air baud rate
	configuration.SPED.uartParity = MODE_00_8N1;			// Parity bit

	configuration.OPTION.subPacketSetting = SPS_200_00;					 // Packet size
	configuration.OPTION.RSSIAmbientNoise = RSSI_AMBIENT_NOISE_DISABLED; // Need to send special command
	configuration.OPTION.transmissionPower = POWER_22;					 // Device power

	configuration.TRANSMISSION_MODE.enableRSSI = RSSI_DISABLED;						 // Enable RSSI info
	configuration.TRANSMISSION_MODE.fixedTransmission = FT_TRANSPARENT_TRANSMISSION; // Enable repeater mode
	configuration.TRANSMISSION_MODE.enableLBT = LBT_DISABLED;						 // Check interference
	configuration.TRANSMISSION_MODE.WORPeriod = WOR_2000_011;						 // WOR timing
	// End of configuration

	// Set configuration changed and set to hold the configuration
	ResponseStatus rs = e220ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
	Serial.println(rs.getResponseDescription());
	Serial.println(rs.code);

	// Create a ResponseStructContainer and fill it with the configuration (again)
	c = e220ttl.getConfiguration();
	// It's important get configuration pointer before all other operation
	configuration = *(Configuration *)c.data;
	
	// Print configuration satus and parameters.
	Serial.println(c.status.getResponseDescription());
	Serial.println(c.status.code);
	printParameters(configuration);

	// Close the ResponseStructContainer c
	c.close();
}

// A function that prints the LoRa module configuration
void printParameters(struct Configuration configuration) 
{
	DEBUG_PRINTLN("----------------------------------------");

	DEBUG_PRINT(F("HEAD : "));  DEBUG_PRINT(configuration.COMMAND, HEX);DEBUG_PRINT(" ");DEBUG_PRINT(configuration.STARTING_ADDRESS, HEX);DEBUG_PRINT(" ");DEBUG_PRINTLN(configuration.LENGHT, HEX);
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("AddH : "));  DEBUG_PRINTLN(configuration.ADDH, HEX);
	DEBUG_PRINT(F("AddL : "));  DEBUG_PRINTLN(configuration.ADDL, HEX);
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("Chan : "));  DEBUG_PRINT(configuration.CHAN, DEC); DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.getChannelDescription());
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("SpeedParityBit     : "));  DEBUG_PRINT(configuration.SPED.uartParity, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.SPED.getUARTParityDescription());
	DEBUG_PRINT(F("SpeedUARTDatte     : "));  DEBUG_PRINT(configuration.SPED.uartBaudRate, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.SPED.getUARTBaudRateDescription());
	DEBUG_PRINT(F("SpeedAirDataRate   : "));  DEBUG_PRINT(configuration.SPED.airDataRate, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.SPED.getAirDataRateDescription());
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("OptionSubPacketSett: "));  DEBUG_PRINT(configuration.OPTION.subPacketSetting, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getSubPacketSetting());
	DEBUG_PRINT(F("OptionTranPower    : "));  DEBUG_PRINT(configuration.OPTION.transmissionPower, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getTransmissionPowerDescription());
	DEBUG_PRINT(F("OptionRSSIAmbientNo: "));  DEBUG_PRINT(configuration.OPTION.RSSIAmbientNoise, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getRSSIAmbientNoiseEnable());
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("TransModeWORPeriod : "));  DEBUG_PRINT(configuration.TRANSMISSION_MODE.WORPeriod, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.TRANSMISSION_MODE.getWORPeriodByParamsDescription());
	DEBUG_PRINT(F("TransModeEnableLBT : "));  DEBUG_PRINT(configuration.TRANSMISSION_MODE.enableLBT, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.TRANSMISSION_MODE.getLBTEnableByteDescription());
	DEBUG_PRINT(F("TransModeEnableRSSI: "));  DEBUG_PRINT(configuration.TRANSMISSION_MODE.enableRSSI, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.TRANSMISSION_MODE.getRSSIEnableByteDescription());
	DEBUG_PRINT(F("TransModeFixedTrans: "));  DEBUG_PRINT(configuration.TRANSMISSION_MODE.fixedTransmission, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.TRANSMISSION_MODE.getFixedTransmissionDescription());


	DEBUG_PRINTLN("----------------------------------------");
}

// A function that prints the LoRa module information
void printModuleInformation(struct ModuleInformation moduleInformation) 
{
	Serial.println("----------------------------------------");
	DEBUG_PRINT(F("HEAD: "));  DEBUG_PRINT(moduleInformation.COMMAND, HEX);DEBUG_PRINT(" ");DEBUG_PRINT(moduleInformation.STARTING_ADDRESS, HEX);DEBUG_PRINT(" ");DEBUG_PRINTLN(moduleInformation.LENGHT, DEC);

	Serial.print(F("Model no.: "));  Serial.println(moduleInformation.model, HEX);
	Serial.print(F("Version  : "));  Serial.println(moduleInformation.version, HEX);
	Serial.print(F("Features : "));  Serial.println(moduleInformation.features, HEX);
	Serial.println("----------------------------------------");

}

// A function that counts all bits in a message
int create_LRC(message_struct message)
{
	int tot = 0;
	
	// Count all bits in the message (except the LRC)
	tot += count_bits(message.soc);
	tot += count_bits(message.pl);
	tot += count_bits(message.src_ID);
	tot += count_bits(message.des_ID);
	tot += count_bits(message.p_ID);
	tot += count_bits(message.functiecode);
	tot += count_bits(message.eot);
	tot += count_bits(message.data.moisture_sensor);
	tot += count_bits(message.data.temperature_sensor);

	for (char i = 0; i < 59; i++)
	{
		tot += count_bits(message.data.pressure_sensor[i]);
		tot += count_bits(message.data.heartbeat[i]);
	}

	return tot;
}

// A function that coults all bits in an acknowledgement message
int create_LRC_ack(response_message_struct message)
{
	int tot = 0;
	
	// Count all bits in the message (except the LRC)
	tot += count_bits(message.soc);
	tot += count_bits(message.pl);
	tot += count_bits(message.src_ID);
	tot += count_bits(message.des_ID);
	tot += count_bits(message.p_ID);
	tot += count_bits(message.functiecode);
	tot += count_bits(message.ack_ID);
	tot += count_bits(message.eot);

	return tot;
}

// A function that counts all bits in a number.
int count_bits(int num)
{
	int tot = 0;

	// While there are still bits in the number
	while (num)
	{
		// total ++ if the LSB of the number is 1.
		if (num & 0x01)
		{
			// if LSB is 1, tot++
			tot++;
		}

		// Shift all bits 1 to the right
		num >>= 1;
	}

	return tot;
}

// A function that gets the LoRa module configuration
void getLoRaConfig()
{
	ResponseStructContainer c;
	c = e220ttl.getConfiguration();
	// It's important get configuration pointer before all other operation
	Configuration configuration = *(Configuration *)c.data;
	Serial.println(c.status.getResponseDescription());
	Serial.println(c.status.code);

	printParameters(configuration);
	c.close();
}

// A function that calculates the rolling average of the last 10 message_times (the time it took to send a message)
int rolling_averagetime()
{
	int sum = 0;
	for(int i = 0; i < 10; i++){
		sum += message_times[i];
	}
	return sum/10;
}