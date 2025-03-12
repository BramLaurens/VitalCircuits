/*
 * LoRa E220
 * Set configuration.
 *
 * You must uncommend the correct constructor.
 *
 * by Renzo Mischianti <https://www.mischianti.org>
 *
 * https://www.mischianti.org
 *
 * E220		  ----- WeMos D1 mini	----- esp32			----- Arduino Nano 33 IoT	----- Arduino MKR	----- Raspberry Pi Pico   ----- stm32               ----- ArduinoUNO
 * M0         ----- D7 (or 3.3v)	----- 19 (or 3.3v)	----- 4 (or 3.3v)			----- 2 (or 3.3v)	----- 10 (or 3.3v)	      ----- PB0 (or 3.3v)       ----- 7 Volt div (or 3.3v)
 * M1         ----- D6 (or 3.3v)	----- 21 (or 3.3v)	----- 6 (or 3.3v)			----- 4 (or 3.3v)	----- 11 (or 3.3v)	      ----- PB10 (or 3.3v)      ----- 6 Volt div (or 3.3v)
 * TX         ----- D3 (PullUP)		----- TX2 (PullUP)	----- TX1 (PullUP)			----- 14 (PullUP)	----- 8 (PullUP)	      ----- PA2 TX2 (PullUP)    ----- 4 (PullUP)
 * RX         ----- D4 (PullUP)		----- RX2 (PullUP)	----- RX1 (PullUP)			----- 13 (PullUP)	----- 9 (PullUP)	      ----- PA3 RX2 (PullUP)    ----- 5 Volt div (PullUP)
 * AUX        ----- D5 (PullUP)		----- 18  (PullUP)	----- 2  (PullUP)			----- 0  (PullUP)	----- 2  (PullUP)	      ----- PA0  (PullUP)       ----- 3 (PullUP)
 * VCC        ----- 3.3v/5v			----- 3.3v/5v		----- 3.3v/5v				----- 3.3v/5v		----- 3.3v/5v		      ----- 3.3v/5v             ----- 3.3v/5v
 * GND        ----- GND				----- GND			----- GND					----- GND			----- GND			      ----- GND                 ----- GND
 *
 */
#define LoRa_E220_DEBUG
#define FREQUENCY_868

#include "Arduino.h"
#include "LoRa_E220.h"

// ---------- esp32 pins --------------
LoRa_E220 e220ttl(&Serial2, 15, 21, 19); //  RX AUX M0 M1


// Define the struct for the message
struct sensordata_struct
{
    unsigned char pressure_sensor[59];        // 0 - 255
    int temperature_sensor;               // -32,768 - 32,767
    short unsigned int moisture_sensor;   // 0 - 65,535
    short int heartbeat[59];                // -32,768 - 32,767
};

struct message_struct
{
  char soc;
  char pl;
  char src_ID;
  char des_ID;
  char pc;
  char functiecode;
  sensordata_struct data;
  int lrc;   
  char eot;
};

void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);
void SetLoRaConfig();

int check_LRC(message_struct message);
int count_bits(int num);

sensordata_struct test_data;
message_struct message;

void setup()
{
	//Set serial for debugging
	Serial.begin(9600);
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

	ResponseStructContainer c;
	c = e220ttl.getConfiguration();
	Configuration configuration = *(Configuration *)c.data;
	// It's important get configuration pointer before all other operation
	Serial.println(c.status.getResponseDescription());
	Serial.println(c.status.code);

	printParameters(configuration);
	c.close();

	// set new serial speed for TXRX
	Serial2.flush();
	Serial2.end();
	Serial2.begin(115200);
}

void loop()
{
	// If something available
	if (e220ttl.available())
	{
		// read the String message
		ResponseStructContainer rsc = e220ttl.receiveMessage(sizeof(message));
		message_struct recieved_message = *(message_struct *)rsc.data;

		// Is something goes wrong print error
		if (rsc.status.code != 1)
		{
			Serial.print("Error with RSC:");
			Serial.println(rsc.status.getResponseDescription());
		}
		else
		{
			// Print the data received

			Serial.print("recieved lrc: ");
			Serial.print(message.lrc);
			Serial.print(" - match with calculated: ");
			Serial.println(check_LRC(recieved_message));

			/*
			
			for (int i = 0; i < 60; i++)
			{
				Serial.println(recieved_message.data.heartbeat[i]);
			}
			Serial.println(recieved_message.des_ID, HEX);
			*/
		}
		rsc.close();
	}
}

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

	// Set configuration changed and set to not hold the configuration
	ResponseStatus rs = e220ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
	Serial.println(rs.getResponseDescription());
	Serial.println(rs.code);
}

void printParameters(struct Configuration configuration) {
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
void printModuleInformation(struct ModuleInformation moduleInformation) {
	Serial.println("----------------------------------------");
	DEBUG_PRINT(F("HEAD: "));  DEBUG_PRINT(moduleInformation.COMMAND, HEX);DEBUG_PRINT(" ");DEBUG_PRINT(moduleInformation.STARTING_ADDRESS, HEX);DEBUG_PRINT(" ");DEBUG_PRINTLN(moduleInformation.LENGHT, DEC);

	Serial.print(F("Model no.: "));  Serial.println(moduleInformation.model, HEX);
	Serial.print(F("Version  : "));  Serial.println(moduleInformation.version, HEX);
	Serial.print(F("Features : "));  Serial.println(moduleInformation.features, HEX);
	Serial.println("----------------------------------------");

}

// A function that counts all bits in the whole message.
int check_LRC(message_struct message)
{
	int tot = 0;

	tot += count_bits(message.soc);
	tot += count_bits(message.pl);
	tot += count_bits(message.src_ID);
	tot += count_bits(message.des_ID);
	tot += count_bits(message.pc);
	tot += count_bits(message.functiecode);
	tot += count_bits(message.lrc);
	tot += count_bits(message.eot);
	tot += count_bits(message.data.moisture_sensor);
	tot += count_bits(message.data.temperature_sensor);

	for (char i = 0; i < 59; i++)
	{
		tot += count_bits(message.data.pressure_sensor[i]);
		tot += count_bits(message.data.heartbeat[i]);
	}

	return tot == message.lrc;
}

// A function that counts all bits in a number.
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
