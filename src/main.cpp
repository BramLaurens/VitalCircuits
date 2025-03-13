
 
#define LoRa_E220_DEBUG
#define FREQUENCY_868
#define messageTimeout 1000
#include "Arduino.h"
#include "LoRa_E220.h"

TaskHandle_t TXRX_task;


// ---------- esp32 pins --------------
LoRa_E220 e220ttl(&Serial2, 15, 21, 19); //  RX AUX M0 M1


// Define the struct for the sensordata
struct sensordata_struct
{
    unsigned char pressure_sensor[59];        // 0 - 255
    int temperature_sensor;               // -32,768 - 32,767
    short unsigned int moisture_sensor;   // 0 - 65,535
    short int heartbeat[59];                // -32,768 - 32,767
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

// Define the struct for the output message
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

// Protoypes
void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);
void SetLoRaConfig(); // Unused
void send_message(message_struct message);
bool ReceiveLoRa();
void getLoRaConfig();
void lora_TXRX(void *pvParameters);

int create_LRC(message_struct message);
int create_LRC_ack(response_message_struct message);
int count_bits(int num);
unsigned long time_last_message_send;

// Create a sensordata struct with testdata, this will later be filled with real sensor data
sensordata_struct test_data;

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


	// -- THE FOLLOWING CODE IS FOR TESTING PURPOSES ONLY --
	// fill test_data
	for (int i = 0; i < 59; i++)
	{
		test_data.pressure_sensor[i] = i;
		test_data.heartbeat[i] = i;
	}
	test_data.temperature_sensor = 12345;
	test_data.moisture_sensor = 54321;


	// Fill message
	message.soc = 0x7E; 									// Self chosen value
	message.src_ID = 0x04;									// Source ID - 0000 0001 In our case (EV1A Group 4)
	message.des_ID = 0x04;									// Destination ID - 0000 0111 In our case (EV1A Group 4)
	message.p_ID = 0x00;									// Package ID - Starts at 0
	message.eot = 0xA4; 									// Self chosen value
	message.pl = sizeof(message) - sizeof(message.lrc);		// Payload length - 1 byte
	message.lrc = create_LRC(message);					// LRC - Integrity check
	// -- THE ABOVE CODE IS FOR TESTING PURPOSES ONLY --


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

	//Create a task to send data and pin it to core 1
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

void lora_TXRX(void *pvParameters) {
    for (;;) { // Infinite loop

		// Wait for response
		if (ReceiveLoRa()) 
		{
			// A message was recieved
			Serial.println("RECIEVED MESSAGE");
			
			// Check if the message is an acknowledge or a retransmit
			// This if statement also checks the LRC, if the LRC isn't correct, the message is ignored.
			if (recieved_message.functiecode == 0x05 && recieved_message.lrc == create_LRC_ack(recieved_message))
			{
				// recieved ackowledge - sending next package

				// update package ID
				message.p_ID++; // NOTE: we start with packet 1. This makes resending packets easier.

				// Debug message
				Serial.print("Recieved ackowledge for package: ");
				Serial.print(recieved_message.p_ID, DEC);
				Serial.print(" - ack_ID: ");
				Serial.print(recieved_message.ack_ID, DEC);
				Serial.print(" - t: (");
				Serial.print(millis());
				Serial.println(")");
				// END Debug message


				// Update data
				message.data = test_data; 		// TODO: Needs to be changed to the real sensor data

				// Set functiecode naar 0x02 (send data)
				message.functiecode = 0x02;

				// Send message
				send_message(message);
				
			}
			else if (recieved_message.functiecode == 0x01 && recieved_message.lrc == create_LRC_ack(recieved_message))
			{
				// recieved retransmit - sending the requested package
				
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
		else if (millis() - time_last_message_send > messageTimeout)
		{
			// Last message is more than 85 ms ago - resend the message

			Serial.print("WARNING - Got no acknowledge for message: ");
			Serial.print(message.p_ID, DEC);
			Serial.println(" - Resending message.");

			// Resend message
			send_message(message);
		}
    }
}

void loop()
{
	
}

// Send a message through the lora module.
void send_message(message_struct message) 
{
	// Save sensor data in buffer
	sensordata_buffer[message.p_ID] = message.data;
	
	// Create LRC
	message.lrc = create_LRC(message);
	
	// Debug message
	Serial.print("Sent message (t: ");
	int timer = millis();
	Serial.print(timer);
	Serial.print(") - Status: ");
	// END Debug message


	// Send message
	// NOTE - This function blocks the program until there is a response
	ResponseStatus rs = e220ttl.sendMessage((uint8_t *)&message, sizeof(message));


	// Debug message
	Serial.print(rs.getResponseDescription());
	Serial.print(" (t: ");
	Serial.print(millis());
	Serial.print(") - Diff: (");
	Serial.print(millis() - timer);
	Serial.print(") ");
	Serial.print(" - p_ID: ");
	Serial.println(message.p_ID, DEC);
	// END Debug message


	// Save the time the message was sent
	time_last_message_send = millis();
}

// Recieve a message from the lora module.
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

// Set configuration variables in the lora module. This is done through the ebyte e220ttl library. (This function is currently not used)
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

// Print the configuration parameters of the lora module. (from LoRa library)
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

// Print module information of the lora module. (from LoRa library)
void printModuleInformation(struct ModuleInformation moduleInformation) 
{
	Serial.println("----------------------------------------");
	DEBUG_PRINT(F("HEAD: "));  DEBUG_PRINT(moduleInformation.COMMAND, HEX);DEBUG_PRINT(" ");DEBUG_PRINT(moduleInformation.STARTING_ADDRESS, HEX);DEBUG_PRINT(" ");DEBUG_PRINTLN(moduleInformation.LENGHT, DEC);

	Serial.print(F("Model no.: "));  Serial.println(moduleInformation.model, HEX);
	Serial.print(F("Version  : "));  Serial.println(moduleInformation.version, HEX);
	Serial.print(F("Features : "));  Serial.println(moduleInformation.features, HEX);
	Serial.println("----------------------------------------");

}

// A function that counts all bits in the acknowledge struct.
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

// A function that counts all bits in the message struct.
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
