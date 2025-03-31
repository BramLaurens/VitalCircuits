// COMMENT OR UNCOMMENT DEBUG STATEMENTS FOR DEBUGGING OPTIONS
	// #define comms_debug
	// #define temp_debug
	// #define moist_debug
	// #define bpm_debug
	// #define rpm_debug
	// #define pressure_debug
	// #define packed_data_debug
	
// END OF DEBUG STATEMENTS

#define LoRa_E220_DEBUG
#define FREQUENCY_868
#define MESSAGE_TIMEOUT 1000
#define ECGR_ARRAY_SIZE 55

#define ECG_PIN 34
#define PRESSURE_PIN 35
#define MOISTURE_PIN 33
#define TEMPERATURE_PIN 32

// BPM definitions
#define BPM_BUFFER_SIZE 10  // Moving average buffer size
#define BPM_PEAK_THRESHOLD_FACTOR 0.8  // Adaptive threshold factor
#define BPM_MIN_RR_INTERVAL 300  // Minimum time between R-peaks (ms)
#define BPM_NOISE_THRESHOLD 500  // Ignore peaks below this value

// RPM definitions
#define RPM_BUFFER_SIZE 10  // Moving average buffer size
#define RPM_PEAK_THRESHOLD_FACTOR 0.8  // Adaptive threshold factor
#define RPM_MIN_RR_INTERVAL 500  // Minimum time between R-peaks (ms)
#define RPM_NOISE_THRESHOLD 2000  // Ignore peaks below this value

#include "Arduino.h"
#include "LoRa_E220.h"
#include <math.h>

TaskHandle_t TXRX_task;

// ---------- esp32 pins --------------
LoRa_E220 e220ttl(&Serial2, 15, 21, 19); // (RX TX) AUX M0 M1


// Define the struct for the sensordata
struct sensordata_struct
{
    unsigned char pressure_sensor[ECGR_ARRAY_SIZE];        // 0 - 255
    float temperature_sensor;                   // -32,768 - 32,767
    unsigned char moisture_sensor;   	  // 0 - 65,535
    short int heartbeat[ECGR_ARRAY_SIZE];				  // -32,768 - 32,767
	char heartbeat_bpm;
	char pressure_rpm;
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

// BPM RPM Counter variables
unsigned long bpm_lastpeakTime = 0;
float bpmBuffer[BPM_BUFFER_SIZE];
int bpm_bufferindex = 0;
bool bpm_bufferfilled = false;
float bpm_peakthreshold = 0;
float bpm = 0;

unsigned long rpm_lastpeakTime = 0;
float rpmBuffer[RPM_BUFFER_SIZE];
int rpm_bufferindex = 0;
bool rpm_bufferfilled = false;
float rpm_peakthreshold = 0;
float rpm = 0;

// Leaky Integrator Variables
float LI_previous = 0.0; // Previous value of the leaky integrator

// Protoypes
void printParameters(struct Configuration configuration);
void printModuleInformation(struct ModuleInformation moduleInformation);
void SetLoRaConfig(); // Unused
void send_message(message_struct message);
bool ReceiveLoRa();
void getLoRaConfig();
void lora_TXRX(void *pvParameters);

void data_samplepack();

double temp_calc();
float moist_calc();

int create_LRC(message_struct message);
int create_LRC_ack(response_message_struct message);
int count_bits(int num);

int rolling_averagetime();

void updateBPMBuffer(float bpm);
void BPM_counter(float rawVal);
float getAverageBPM();

void updateRPMBuffer(float rpm);
void RPM_counter(float rawVal);
float getAverageRPM();

float LI_filter(float rawValue);

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

			// Reset variables, stop task 0 from putting new data in the live buffer
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

		// Wait for response (acknowledge or retransmit)
		if (ReceiveLoRa()) 
		{
			// A message was recieved
			#ifdef comms_debug
				Serial.println(" ");
			#endif
			
			// Check if the message is an acknowledge or a retransmit
			// This if statement also checks the ack LRC, if the ack LRC isn't correct, the message is ignored.
			if (recieved_message.functiecode == 0x05 && recieved_message.lrc == create_LRC_ack(recieved_message))
			{

				#ifdef comms_debug
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
				#endif

				//Mark verification routine as done
				lastmessage_done = true;
				

			}

			// if recieved retransmit - sending the requested package
			else if (recieved_message.functiecode == 0x01 && recieved_message.lrc == create_LRC_ack(recieved_message))
			{
				#ifdef comms_debug
					// Debug message
					Serial.print("Recieved retramsnit request for package: ");
					Serial.print(recieved_message.ack_ID, DEC);
					Serial.print(" - t: (");
					Serial.print(millis());
					Serial.println(")");
					// END Debug message
				#endif

				
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
	data_samplepack();
}

void data_samplepack(){
	// Collect, process and pack sensor data in live_data struct
	// Calculate optimal sample interval for heartbeat sensor based on the rolling average latency of the last 10 messages
	datasample_interval = rolling_averagetime() / ECGR_ARRAY_SIZE;

	// Wait for data to be pulled in task 0
	if(data_pulled)
	{
		#ifdef packed_data_debug
			Serial.print("Packed data in live: ");
		#endif

		//Mark that new data is not available right now so that TXRX wont send the same data again
		newdata_available = false;

		// Collect sensor data when last message is sent and acknowledged
		for(int i = 0; i < ECGR_ARRAY_SIZE; i++)
		{
			float ecgRaw = analogRead(ECG_PIN);
			float pressureRaw = analogRead(PRESSURE_PIN);
			BPM_counter(ecgRaw);
			getAverageBPM();

			RPM_counter(pressureRaw);
			getAverageRPM();

			live_data.heartbeat[i] = map(ecgRaw, 0, 4095, 0, 32767);
			live_data.pressure_sensor[i] = map(LI_filter(pressureRaw), 0, 4095, 0, 255);

			delay(datasample_interval);
		}

		live_data.moisture_sensor = moist_calc();
		live_data.temperature_sensor = temp_calc();
		live_data.heartbeat_bpm = getAverageBPM();
		live_data.pressure_rpm = getAverageRPM();

		#ifdef packed_data_debug
			Serial.print(" Moist: ");
			Serial.print(live_data.moisture_sensor);
			Serial.print(" Temp: ");
			Serial.print(live_data.temperature_sensor);
			Serial.print(" BPM: ");
			Serial.println(live_data.heartbeat_bpm, DEC);
		#endif

		// Flag new data as available and not pulled yet so that TXRX can send the data
		newdata_available = true;
		data_pulled = false;
	}
}

float moist_calc(){
	int moist_raw = analogRead(MOISTURE_PIN);
	int moist_perc = map(moist_raw, 1000, 1300, 100, 0);

	if(moist_perc < 0)
	{
		moist_perc = 0;
	}
	else if(moist_perc > 100)
	{
		moist_perc = 100;
	}

	#ifdef moist_debug
		Serial.print("moist_raw: ");
		Serial.print(moist_raw);
		Serial.print("	moist_perc: ");
		Serial.println(moist_perc);
	#endif

	return moist_perc;
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

	for (char i = 0; i < ECGR_ARRAY_SIZE; i++)
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

void BPM_counter(float rawVal){
    static float lastValue = 0;
    static float maxECG = 0;
    unsigned long currentTime = millis();
	static float lastResettime = 0;
    
    // Read ECG signal
    
	// Reset maxECG every 2 seconds
	if (currentTime - bpm_lastpeakTime > 2000 && maxECG > 0 && currentTime - lastResettime > 2000) {  
		maxECG = 0;
	}

    // Update peak threshold dynamically
    if (rawVal > maxECG) {
        maxECG = rawVal;
        bpm_peakthreshold = maxECG * BPM_PEAK_THRESHOLD_FACTOR;
    }
    
    // Reset BPM after 5 seconds of inactivity
    if (currentTime - bpm_lastpeakTime > 4000) {  
        bpm = 0;
        updateBPMBuffer(bpm);
    }

    // Detect peaks
    if (rawVal > bpm_peakthreshold && rawVal > BPM_NOISE_THRESHOLD && lastValue <= bpm_peakthreshold) {
        unsigned long rrInterval = currentTime - bpm_lastpeakTime;
        if (rrInterval > BPM_MIN_RR_INTERVAL) {  // Ignore noise and too-fast beats
            bpm = 60000.0 / rrInterval;
            updateBPMBuffer(bpm);
            bpm_lastpeakTime = currentTime;
        }
    }
    
    lastValue = rawVal;

	#ifdef bpm_debug
		Serial.print("Max ECG: ");
		Serial.print(maxECG);
		Serial.print("Instant BPM: ");
		Serial.print(bpm);
		Serial.print("  ||  Average BPM: ");
		Serial.println(getAverageBPM());
	#endif
}

void updateBPMBuffer(float bpm) {
    bpmBuffer[bpm_bufferindex] = bpm;
    bpm_bufferindex = (bpm_bufferindex + 1) % BPM_BUFFER_SIZE;
    if (bpm_bufferindex == 0) bpm_bufferfilled = true;
}

float getAverageBPM() {
    float sum = 0;
    int count;
    if (!bpm_bufferfilled){
      count = bpm_bufferindex;
    }
    else {
      count = BPM_BUFFER_SIZE;
    }

    for (int i = 0; i < count; i++) {
        sum += bpmBuffer[i];
    }
    
    if(count > 0){
      return sum / count;
    }
    else{
      return 0;
    }
}

void RPM_counter(float rawVal){
    static float lastValue = 0;
    static float maxPressure = 0;
	static float lastResettime = 0;
    unsigned long currentTime = millis();
    
    // Read ECG signal
    float pressureValue = 4096 - LI_filter(rawVal); // Filter and invert the value to get pressure
    
	// Reset maxECG every 5 seconds
    if (currentTime - rpm_lastpeakTime > 5000 && maxPressure > 0 && currentTime - lastResettime > 5000) {  
        maxPressure = 0;
		lastResettime = currentTime;
    }
	

    // Update peak threshold dynamically
    if (pressureValue > maxPressure) {
        maxPressure = pressureValue;
        rpm_peakthreshold = maxPressure * RPM_PEAK_THRESHOLD_FACTOR;
    }

    // Reset BPM after 10 seconds of inactivity
    if (currentTime - rpm_lastpeakTime > 10000) {  
        rpm = 0;
        updateRPMBuffer(rpm);
    }
	

    // Detect peaks
    if (pressureValue > rpm_peakthreshold && pressureValue > RPM_NOISE_THRESHOLD && lastValue <= rpm_peakthreshold) {
		#ifdef rpm_debug
			Serial.print(" Peak detected: ");
		#endif

        unsigned long rrInterval = currentTime - rpm_lastpeakTime;
        if (rrInterval > RPM_MIN_RR_INTERVAL) {  // Ignore noise and too-fast beats
            rpm = 60000.0 / rrInterval;
            updateRPMBuffer(rpm);
            rpm_lastpeakTime = currentTime;
        }
    }
    
    lastValue = pressureValue;

	#ifdef rpm_debug
		Serial.print("Max pressure: ");
		Serial.print(maxPressure);
		Serial.print("	Peak treshold: ");
		Serial.print(rpm_peakthreshold);
		Serial.print("	Pressure: ");
		Serial.print(pressureValue);
		Serial.print("	Instant RPM: ");
		Serial.print(rpm);
		Serial.print("  ||  Average RPM: ");
		Serial.println(getAverageRPM());
	#endif
}

void updateRPMBuffer(float rpm) {
    rpmBuffer[rpm_bufferindex] = rpm;
    rpm_bufferindex = (rpm_bufferindex + 1) % RPM_BUFFER_SIZE;
    if (rpm_bufferindex == 0) rpm_bufferfilled = true;
}

float getAverageRPM() {
    float sum = 0;
    int count;
    if (!rpm_bufferfilled){
      count = rpm_bufferindex;
    }
    else {
      count = RPM_BUFFER_SIZE;
    }

    for (int i = 0; i < count; i++) {
        sum += rpmBuffer[i];
    }
    
    if(count > 0){
      return sum / count;
    }
    else{
      return 0;
    }
}

float LI_filter(float rawValue) {
	float alpha = 0.99; // Higher alpha = more extreme smoothing

	// Apply the leaky integrator formula
	float filtered_val = rawValue * (1-alpha) + LI_previous * alpha;
	LI_previous = filtered_val; // Store the current value for the next iteration

	return filtered_val;
}