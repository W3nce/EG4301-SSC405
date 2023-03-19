
/*********  
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/


// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
//#define SERVICE_UUID2 "30880f64-4239-11ed-b878-0242ac120002"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAddress.h>
#include <BLEUtils.h>
#include <BLE2902.h>  
#include <string>
#include <WiFi.h>
#define ServerAddressLength 17

// #define use_BME280
#define use_SHT3X
// #define use_DHT22

#ifdef use_BME280
#include "BME280_utils.h"
  #define temp BME280_Temperature
  #define hum BME280_Humidity
  #define Sensor_Start BME280_Start
  #define getValues getBMEValues
#endif

#ifdef use_SHT3X
#include "SHT3X_utils.h"
  #define temp SHT3X_Temperature
  #define hum SHT3X_Humidity
  #define Sensor_Start SHT3X_Start
  #define getValues getSHTValues
#endif

#ifdef use_DHT22
#include "DHT22_utils.h"
  #define temp DHT22_Temperature
  #define hum DHT22_Humidity
  #define Sensor_Start DHT22_Start
  #define getValues getDHTValues
#endif

//Default Temperature is in Celsius
#define temperatureCelsius

//BLE server name
std::string bleServerName = WiFi.macAddress().c_str();
//"SHT";


//Deep Sleep Time/ logging frequency
#define uS_TO_S_FACTOR      1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP       30          /* Time ESP32 will go to sleep (in seconds) */

//First time Authentication
RTC_FAST_ATTR static int Authenticated = false;

//BLE Server Object
BLEServer *pServer;

// Timer variables
unsigned long lastTime = 0;

//Logging Checking Interval in millis
unsigned long loggingInterval = 1000;

//Check if Client connected
static bool deviceConnected = false;

//BLE Serice UUID
#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59"

// Temperature Characteristic and Descriptor
#ifdef temperatureCelsius
  BLECharacteristic BLETemperatureCelsiusCharacteristics("cba1d466-344c-4be3-ab3f-189f80dd7518", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor BLETemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2902));
#else
  BLECharacteristic BLETemperatureFahrenheitCharacteristics("f78ebbff-c8b7-4107-93de-889a6a06d408", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor BLETemperatureFahrenheitDescriptor(BLEUUID((uint16_t)0x2902));
#endif

// Humidity Characteristic and Descriptor
BLECharacteristic BLEHumidityCharacteristics("ca73b3ba-39f6-4ab3-91ae-186dc9577d99", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor BLEHumidityDescriptor(BLEUUID((uint16_t)0x2903));

//returns Boolean String
std::string boolString(bool b) {
  if (b) {
    return "true";
  } else {
    return "false";
  }
}

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("ServerCallBack: onConnect");
    deviceConnected = true;
  };
  //Start advertise for new connection after disconnect
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    // pServer->getAdvertising()->start();
    Serial.println("ServerCallBack: onDisconnect: Going to Deep Sleep...");
    Serial1.println("ServerCallBack: onDisconnect: Going to Deep Sleep...");
    //Ping that going to sleep
    //goes to sleep
    Serial.flush();  
    Serial1.flush();  
    esp_deep_sleep_start();

  }
};

void StartService(){
  //New Name
  std::string myServerName = WiFi.macAddress().c_str();

  // Create the BLE Device
  BLEDevice::init(myServerName);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *BLEService = pServer->createService(SERVICE_UUID);

  std::string ServerAddress = BLEDevice::getAddress().toString();
  std::string BLETempDescriptor =  "_Temperature Celsius";
  std::string BLETempDescriptorF = "_Temperature Fahrenheit";
  std::string BLEHumDescriptor = "Humididty";

  // Create BLE Characteristics and Create a BLE Descriptor
  // Temperature
  #ifdef temperatureCelsius
    BLEService->addCharacteristic(&BLETemperatureCelsiusCharacteristics);
    BLETemperatureCelsiusDescriptor.setValue(BLETempDescriptor);
    BLETemperatureCelsiusCharacteristics.addDescriptor(new BLE2902());
  #else
    BLEService->addCharacteristic(&BLETemperatureFahrenheitCharacteristics);
    BLETemperatureFahrenheitDescriptor.setValue(BLETempDescriptorF);
    BLETemperatureFahrenheitCharacteristics.addDescriptor(new BLE2902());
  #endif  

  // Humidity
  BLEService->addCharacteristic(&BLEHumidityCharacteristics);
  BLEHumidityDescriptor.setValue(BLEHumDescriptor);
  BLEHumidityCharacteristics.addDescriptor(new BLE2902());
  
  // Start the service
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  BLEService->start();
}

void SetNotify(float &rtemp, float &rhum){

  // Fahrenheit
  float tempFar = 1.8*rtemp +32;
  //Notify temperature reading
  #ifdef temperatureCelsius
    static char temperatureCTemp[6];
    dtostrf(rtemp, 6, 2, temperatureCTemp);
    //Set temperature Characteristic value and notify connected client
    BLETemperatureCelsiusCharacteristics.setValue(temperatureCTemp);
    BLETemperatureCelsiusCharacteristics.notify();
    Serial.print("Temperature Celsius: ");
    Serial.print(rtemp);
    Serial.print(" deg C");
    Serial.print("  <");
    Serial.print(&temperatureCTemp[0]);
    Serial.print(">  ");
    Serial.print(sizeof(temperatureCTemp));
  #else
    static char temperatureFTemp[6];
    dtostrf(tempFar, 6, 2, temperatureFTemp);
    //Set temperature Characteristic value and notify connected client
    BLETemperatureFahrenheitCharacteristics.setValue(temperatureFTemp);
    BLETemperatureFahrenheitCharacteristics.notify();
    Serial.print("Temperature Fahrenheit: ");
    Serial.print(tempFar);
    Serial.print(" ºF");
    Serial.print("  <");
    Serial.print(&temperatureFTemp[0]);
    Serial.print(">");
  #endif
  
  //Notify humidity reading
  static char humidityTemp[7];
  dtostrf(rhum, 6, 2, humidityTemp);
  //Set humidity Characteristic value and notify connected client
  BLEHumidityCharacteristics.setValue(humidityTemp);
  BLEHumidityCharacteristics.notify();   
  Serial.print(" - Humidity: ");
  Serial.print(rhum);
  Serial.print(" %");
  Serial.print("  <");
  Serial.print(&humidityTemp[0]);
  Serial.println(">");
}

bool sendSerialData(std::string SerialData, int TimeOutMS){
  int CharactersRead;
  std::string CurrentStatus =                       "00000000000000000";
  std::string OkStatus =                            "11111111111111111";
  std::string TimeOut =                             "99999999999999999";
  int count = 0 ;
  Serial1.println(SerialData.c_str());

  while (CharactersRead != ServerAddressLength && count < TimeOutMS ){
    char SerialRecieved[ServerAddressLength + 1] =  "00000000000000000";
    CharactersRead = Serial1.readBytesUntil('\n', SerialRecieved,ServerAddressLength);
    CurrentStatus = SerialRecieved;
    Serial1.flush();
    delay(50);
    count += 50;
    if (CurrentStatus == OkStatus){
      return true;
    }
  }

  Serial1.println(TimeOut.c_str());
  return false;
}

bool Authenticate(){
  std::string CurrentStatus =                       "00000000000000000";
  std::string OkStatus =                            "11111111111111111";
  std::string FinishStatus =                        "22222222222222222";
  int CharactersRead;

  while (!( CharactersRead == ServerAddressLength &&
            CurrentStatus == OkStatus)
  ){
    Serial1.println("MACADDRESSAUTHENT");
    CurrentStatus =                                 "00000000000000000";
    char SerialRecieved[ServerAddressLength + 1] =  "00000000000000000";
    CharactersRead = Serial1.readBytesUntil('\n', SerialRecieved,ServerAddressLength);
    CurrentStatus = SerialRecieved;
    Serial.print("Recieved Status:");
    Serial.print(SerialRecieved);
    Serial.println(" (" + String(CharactersRead) + ")");
    Serial1.flush();
  }

  Serial.println("Beacon Recognised");


  while (!( CharactersRead == ServerAddressLength &&
            CurrentStatus == FinishStatus)
  ){
    Serial1.println(WiFi.macAddress());
    CurrentStatus =                                 "00000000000000000";
    char SerialRecieved[ServerAddressLength + 1] =  "00000000000000000";
    CharactersRead = Serial1.readBytesUntil('\n', SerialRecieved,ServerAddressLength);
    CurrentStatus = SerialRecieved;
    Serial.print("Recieved Status:");
    Serial.print(SerialRecieved);
    Serial.println(" (" + String(CharactersRead) + ")");
    Serial1.flush();
  }

  Serial.println("Beacon Server Registered");

  //Register Sleep Time
  return true;
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); Serial1.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); Serial1.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); Serial1.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad");Serial1.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); Serial1.println("Wakeup caused by ULP program");break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); Serial1.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void setup() {
  // Start serial communication 
  Serial1.begin(UART_BAUD, SERIAL_8N1, BOARD_RX, BOARD_TX);
  // Serial.begin(UART_BAardUD);
  Serial1.println("Woke UP");
  //Prints WakeUp Reason
  print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR ); 

  Serial.print("Device Name: ");
  Serial.println(WiFi.macAddress().c_str());

  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  Serial.println(Authenticated ? "Authenticated = true" : "Authenticated = false");
  Serial1.println(Authenticated ? "Authenticated = true" : "Authenticated = false");
  if (!Authenticated){
  Authenticated = Authenticate();
  }

  // Init BME Sensor/ DHT22
  Sensor_Start();
  //dht.begin();

  StartService();

  // Start Temporary Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();

  Serial.printf("Server Name: %s \n",bleServerName.c_str());
  Serial.println("Waiting a client connection to notify...");
  Serial1.println("Waiting a client connection to notify...");
}


void loop() {
  //Serial.println(boolString(deviceConnected).c_str());
  if (deviceConnected) {
    if ((millis() - lastTime) > loggingInterval) {
      //Serial.println("start");
      Serial.println("Start Reading...");

      // Read temperature as Celsius (the default)
      getValues();

      SetNotify(temp,hum);

      lastTime = millis();
      //Serial.println("end");
    }
  }
  delay(1000);
  
}


