#define FREQUENCY_868
#define LoRa_E220_DEBUG

#include <Arduino.h>
#include <RH_ASK.h>
#include <SPI.h> // Not actually used but needed to compile
#include <HardwareSerial.h>
#include <EByte_LoRa_E220_library.h>

#define RX_GPIO 18
#define TX_GPIO 19

HardwareSerial LoRaPort(2);
LoRa_E220 LoraTTL(&LoRaPort);

void printLoRaParameters(struct Configuration configuration);
void printLoRaModuleInformation(struct ModuleInformation moduleInformation);
void LoRaSetConfig();

struct testData {
  int testint;
  float testfloat;
  bool testbool;
};

testData testdata = {1, 2.3, true};

void setup()
{
  //Serial setup
  Serial.begin(115200);
  while(!Serial){};
  delay(500);
  Serial.println();
  LoRaPort.begin(115200, SERIAL_8N1, RX_GPIO, TX_GPIO);

  LoRaSetConfig();

  LoraTTL.begin();
}

void loop()
{
  static unsigned long lastSend = millis();
  if (millis() - lastSend > 1000) {
    lastSend = millis();
    LoraTTL.sendMessage(&testdata, sizeof(testdata));
  }

}

void LoRaSetConfig(){
  ResponseStructContainer c; //Make a struct container for the response from event on the LoRa module (data, rssi, status)
  c = LoraTTL.getConfiguration(); //Get the current configuration of the LoRa module and store the return in the struct container
  Configuration LoraConfig = *(Configuration *) c.data; //Type cast to make sure the data is actually stored in the config struct
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);

  LoraConfig.ADDL = 0x02;  // Low byte of address
  LoraConfig.ADDH = 0x00; // High byte of address
  LoraConfig.CHAN = 18;   // Channel

  LoraConfig.SPED.uartBaudRate = UART_BPS_9600;
  LoraConfig.SPED.uartParity = MODE_00_8N1;
  LoraConfig.SPED.airDataRate = AIR_DATA_RATE_010_24;

  LoraConfig.OPTION.subPacketSetting = SPS_200_00;
  LoraConfig.OPTION.RSSIAmbientNoise = RSSI_AMBIENT_NOISE_DISABLED;
  LoraConfig.OPTION.transmissionPower = POWER_22;

  LoraConfig.TRANSMISSION_MODE.enableRSSI = RSSI_DISABLED;
  LoraConfig.TRANSMISSION_MODE.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
  LoraConfig.TRANSMISSION_MODE.enableLBT = LBT_DISABLED;
  LoraConfig.TRANSMISSION_MODE.WORPeriod = WOR_2000_011;

  LoraConfig.CRYPT.CRYPT_H = 0x00;
  LoraConfig.CRYPT.CRYPT_L = 0x00;

  ResponseStatus LoRaRS = LoraTTL.setConfiguration(LoraConfig, WRITE_CFG_PWR_DWN_SAVE);
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);

  printLoRaParameters(LoraConfig);
  c.close(); //Close and clear the struct container

}

void printLoRaParameters(struct Configuration configuration) {
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

void printLoRaModuleInformation(struct ModuleInformation moduleInformation) {
  Serial.println("----------------------------------------");
  DEBUG_PRINT(F("HEAD: "));  DEBUG_PRINT(moduleInformation.COMMAND, HEX);DEBUG_PRINT(" ");DEBUG_PRINT(moduleInformation.STARTING_ADDRESS, HEX);DEBUG_PRINT(" ");DEBUG_PRINTLN(moduleInformation.LENGHT, DEC);
  Serial.print(F("Model no.: "));  Serial.println(moduleInformation.model, HEX);
  Serial.print(F("Version  : "));  Serial.println(moduleInformation.version, HEX);
  Serial.print(F("Features : "));  Serial.println(moduleInformation.features, HEX);
  Serial.println("----------------------------------------");
}