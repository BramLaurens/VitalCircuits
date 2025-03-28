#define LoRa_E220_DEBUG
#define FREQUENCY_868
#define MESSAGE_TIMEOUT 200

#include "Arduino.h"
#include "LoRa_E220.h"

#define ECGR_ARRAY_SIZE 58

TaskHandle_t data_reciever_task;

// ---------- esp32 pins --------------
LoRa_E220 e220ttl(&Serial2, 15, 21, 19); //  (RX TX) AUX M0 M1


// Define struct for sensor data
struct sensordata_struct
{
    unsigned char pressure_sensor[ECGR_ARRAY_SIZE];        // 0 - 255
    int temperature_sensor;                   // -32,768 - 32,767
    unsigned char moisture_sensor;   	  // 0 - 65,535
    short int heartbeat[ECGR_ARRAY_SIZE];				  // -32,768 - 32,767
	char heartbeat_bpm;
};

// Define struct for message
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

// Define struct for acknowledgement message
struct message_ack 
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

// initialize structs
message_struct recieved_message;
message_struct verifiedMessage;
message_ack ack_message;

//Prototypes
void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);
void SetLoRaConfig();
bool ReceiveLoRa();
void GetLoRaConfig();
void acknowledgeLoRa();
void send_message_ack(message_ack message);
bool verify_message();
void retransmitReq();

void reciever_loop(void *pvParameters);

int create_LRC(message_struct message);
int create_LRC_ACK(message_ack message);
int count_bits(int num);

// Variables
unsigned long lastSendTime = 0;
char timeoutMode = 0;
bool message_recieved = false;

void setup()
{
	//Set serial for debugging
	Serial.begin(115200);
	//Set serial for configuration
	Serial2.begin(9600, SERIAL_8N1, 16, 17);
	while (!Serial)
	{
	};
	delay(500);

	Serial.println();

	// Startup all pins and UART
	e220ttl.begin();
	ResponseStatus rs = e220ttl.resetModule();
	Serial.println("Resetting:");
	Serial.println(rs.getResponseDescription());
	Serial.println(rs.code);
	e220ttl.setMode(MODE_0_NORMAL);

	//Set LoRa module config
	//SetLoRaConfig();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
	GetLoRaConfig();

	//Create a task to send data and pin it to core 1
	xTaskCreatePinnedToCore(
		reciever_loop, 
		"data_reciever_task",
		10000,
		NULL,
		1,
		&data_reciever_task,
		0
	);  

	// set new serial speed for TXRX
	Serial2.flush();
	Serial2.end();
	Serial2.begin(115200);
}

// Main Loop, handles printing of sensor data for processing
void loop()
{
	if (message_recieved)
	// If a message is recieved, verified and not yet processed, print all heartbeat values.
	{
		for (char i = 0; i < ECGR_ARRAY_SIZE; i++) 
		{
			Serial.print("h");
			Serial.println(verifiedMessage.data.heartbeat[i]);
			
			Serial.print("p");
			Serial.println(verifiedMessage.data.pressure_sensor[i]);
			delayMicroseconds(2000);
		}

		Serial.print("t");
		Serial.println(verifiedMessage.data.temperature_sensor);
		Serial.print("b");
		Serial.println(verifiedMessage.data.heartbeat_bpm, DEC);
		Serial.print("m");
		Serial.println(verifiedMessage.data.moisture_sensor, DEC);
		message_recieved = false;
	}
}

// Reciever loop, handles recieving messages, verifying them, and sending acknowledgements
void reciever_loop(void *pvParameters) 
{
	for (;;) // Infinite loop
	{ 
		//If received message, fill temp struct, and verify
		if(ReceiveLoRa())
		{
			
			if(verify_message())
			{
				// Message verified, send acknowledgement
				//Serial.println("Verification successful, sending acknowledgement");
				acknowledgeLoRa();
				lastSendTime = millis();
				//timeoutMode = true > timeout for acknowledgement
				timeoutMode = 1;
				verifiedMessage = recieved_message;
				message_recieved = true;
			}
			else
			{
				// Request retransmit
				//Serial.println("Verification failed, requesting retransmit");
				retransmitReq();
				lastSendTime = millis();
				//timeoutMode = false > timeout for retransmit request
				timeoutMode = 2;
			}
		}
		
		// Send either ack or retransmit req if not hearing anything
		if ((millis() - lastSendTime )> MESSAGE_TIMEOUT)
		{
			switch (timeoutMode)
			{
			case 1:
				//Serial.println("Timeout, resending acknowledgement");
				acknowledgeLoRa();
				lastSendTime = millis();
				break;
			
			case 2:
				//Serial.println("Timeout, resending retransmit request");
				acknowledgeLoRa();
				lastSendTime = millis();
			default:
				break;
			}
		}

		// Add delay to not overload the task
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

// Set LoRa module configuration
// This function is not called, since the LoRa module is already configured and set to remember the configuration
void SetLoRaConfig()
{
	ResponseStructContainer c;
	c = e220ttl.getConfiguration();
	// It's important get configuration pointer before all other operation
	Configuration configuration = *(Configuration *)c.data;
	Serial.println(c.status.getResponseDescription());
	Serial.println(c.status.code);

	printParameters(configuration);

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

	// Set configuration changed and set to hold the configuration
	ResponseStatus rs = e220ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
	Serial.println(rs.getResponseDescription());
	Serial.println(rs.code);
}

// A function that gets the LoRa module configuration
void GetLoRaConfig()
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

// A function that receives a message from the LoRa module
bool ReceiveLoRa()
{
	// If something available
	if (e220ttl.available())
	{
		//Serial.println("Received message:");
		// read the String message
		ResponseStructContainer rsc = e220ttl.receiveMessage(sizeof(message_struct));
		recieved_message = *(message_struct *)rsc.data;

		// If something goes wrong print error
		if (rsc.status.code != 1)
		{
			//Serial.print("Error with RSC:");
			//Serial.println(rsc.status.getResponseDescription());
		}
		else
		{
			// Print the data received
			/*
			Serial.print(" - Time: ");
			Serial.print(millis());
			Serial.print(" - RSSI: ");
			Serial.print(rsc.rssi);
			Serial.print(" - received p_ID: ");
			Serial.print(recieved_message.p_ID, DEC);
			Serial.print(" - recieved src_ID: ");
			Serial.print(recieved_message.src_ID, DEC);
			Serial.print(" - received des_ID: ");
			Serial.print(recieved_message.des_ID, DEC);
			Serial.print(" - recieved lrc: ");
			Serial.print(recieved_message.lrc);
			Serial.print(" - match with calculated: ");
			Serial.print(create_LRC(recieved_message) == recieved_message.lrc);
			Serial.print(" - Temperature sensor val: ");
			Serial.print(recieved_message.data.temperature_sensor);

			Serial.print(" - Calculated LRC:");
			Serial.println(create_LRC(recieved_message));
			*/
		}
		rsc.close();
		return true;
	}
	return false;
}

// A function that counts all bits in a message
int create_LRC(message_struct message)
{
	int calculatedLRC = 0;
	calculatedLRC += count_bits(message.soc);
	calculatedLRC += count_bits(message.pl);
	calculatedLRC += count_bits(message.src_ID);
	calculatedLRC += count_bits(message.des_ID);
	calculatedLRC += count_bits(message.p_ID);
	calculatedLRC += count_bits(message.functiecode);
	calculatedLRC += count_bits(message.eot);
	calculatedLRC += count_bits(message.data.moisture_sensor);
	calculatedLRC += count_bits(message.data.temperature_sensor);

	for (char i = 0; i < ECGR_ARRAY_SIZE; i++)
	{
		calculatedLRC += count_bits(message.data.pressure_sensor[i]);
		calculatedLRC += count_bits(message.data.heartbeat[i]);
	}

	return calculatedLRC;
}

// A function that coults all bits in an acknowledgement message
int create_LRC_ACK(message_ack message)
{
	int calculatedLRC = 0;
	calculatedLRC += count_bits(message.soc);
	calculatedLRC += count_bits(message.pl);
	calculatedLRC += count_bits(message.src_ID);
	calculatedLRC += count_bits(message.des_ID);
	calculatedLRC += count_bits(message.p_ID);
	calculatedLRC += count_bits(message.functiecode);
	calculatedLRC += count_bits(message.eot);
	calculatedLRC += count_bits(message.ack_ID);

	return calculatedLRC;
}

// A function that counts all bits in a number
int count_bits(int num)
{
	int tot = 0;

	while (num)
	{
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

// A function that verifies a message by checking the start of communication, destination ID, source ID and LRC
bool verify_message()
{

	if(recieved_message.soc != 0x7E){
		//Serial.println(" - Start of communication not correct");
		return false;
	}
	if(recieved_message.des_ID != 0x04){
		//Serial.println(" - Destination ID not correct");
		return false;
	}

	if(recieved_message.src_ID != 0x04){
		//Serial.println(" - Source ID not correct");
		return false;
	}

	if(create_LRC(recieved_message) != recieved_message.lrc){
		//Serial.println(" - LRC not correct");
		return false;
	}

	return true;
}

// A function that creates and sends an acknowledgement message
void acknowledgeLoRa()
{
	ack_message.soc = 0x7E;
	ack_message.pl = sizeof(ack_message) - sizeof(ack_message.lrc);
	ack_message.functiecode = 0x05;
	ack_message.src_ID = 0x04;
	ack_message.des_ID = 0x04;
	ack_message.p_ID = 0x01;
	ack_message.ack_ID = recieved_message.p_ID;
	ack_message.lrc = create_LRC_ACK(ack_message);

	send_message_ack(ack_message);
}

// A function that creates and sends a retransmit request
void retransmitReq()
{
	ack_message.soc = 0x7E;
	ack_message.pl = sizeof(ack_message) - sizeof(ack_message.lrc);
	ack_message.functiecode = 0x01;
	ack_message.src_ID = 0x04;
	ack_message.des_ID = 0x04;
	ack_message.p_ID = 0x01;
	ack_message.ack_ID = recieved_message.p_ID;
	ack_message.lrc = create_LRC_ACK(ack_message);

	send_message_ack(ack_message);
}

// A function that sends a message back to the transmitter
void send_message_ack(message_ack message)
{
	ResponseStatus rs = e220ttl.sendMessage((uint8_t *)&message, sizeof(message_ack));
	/*
	Serial.print("Sent acknowledgement:	");
	Serial.print(ack_message.ack_ID, DEC);
	Serial.print("	");
	Serial.print(rs.getResponseDescription());
	Serial.print("	");
	Serial.println(rs.code);
	Serial.println("	");
	*/
}