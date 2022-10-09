
/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEAddress.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <string>

//Default Temperature is in Celsius
//Comment the next line for Temperature in Fahrenheit
#define temperatureCelsius

//BLE server name
#define bleServerName "Child_1"
//BLE server name 2
//#define bleServerName "Child_1"

Adafruit_BME280 bme; // I2C

float temp;
float tempF;
float hum;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

bool deviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59"

//#define SERVICE_UUID2 "30880f64-4239-11ed-b878-0242ac120002"

// Temperature Characteristic and Descriptor
#ifdef temperatureCelsius
  BLECharacteristic bmeTemperatureCelsiusCharacteristics("cba1d466-344c-4be3-ab3f-189f80dd7518", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor bmeTemperatureCelsiusDescriptor(BLEUUID((uint16_t)0x2902));
#else
  BLECharacteristic bmeTemperatureFahrenheitCharacteristics("f78ebbff-c8b7-4107-93de-889a6a06d408", BLECharacteristic::PROPERTY_NOTIFY);
  BLEDescriptor bmeTemperatureFahrenheitDescriptor(BLEUUID((uint16_t)0x2902));
#endif

// Humidity Characteristic and Descriptor
BLECharacteristic bmeHumidityCharacteristics("ca73b3ba-39f6-4ab3-91ae-186dc9577d99", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor bmeHumidityDescriptor(BLEUUID((uint16_t)0x2903));


//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    delay(1000);
    pServer->getAdvertising()->start();
    Serial.println("Persisiting Advertising for a client connection to notify...");
  }
};

void initBME(){
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
}

int show = 0;
BLEServer *pServer;

void setup() {
  // Start serial communication 
  Serial.begin(115200);

  // Init BME Sensor
  initBME();

  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *bmeService = pServer->createService(SERVICE_UUID);

  std::string ServerAddress = BLEDevice::getAddress().toString();
  std::string bmeTempDescriptor =  "_BME temperature Celsius";
  std::string bmeTempDescriptorF = "_BME temperature Fahrenheit";
  std::string bmeHumDescriptor = "_BME temperature Celsius";

  // Create BLE Characteristics and Create a BLE Descriptor
  // Temperature
  #ifdef temperatureCelsius
    bmeService->addCharacteristic(&bmeTemperatureCelsiusCharacteristics);
    bmeTemperatureCelsiusDescriptor.setValue(bmeTempDescriptor);
    bmeTemperatureCelsiusCharacteristics.addDescriptor(new BLE2902());
  #else
    bmeService->addCharacteristic(&bmeTemperatureFahrenheitCharacteristics);
    bmeTemperatureFahrenheitDescriptor.setValue(bmeTempDescriptorF);
    bmeTemperatureFahrenheitCharacteristics.addDescriptor(new BLE2902());
  #endif  

  // Humidity
  bmeService->addCharacteristic(&bmeHumidityCharacteristics);
  bmeHumidityDescriptor.setValue(bmeHumDescriptor);
  bmeHumidityCharacteristics.addDescriptor(new BLE2902());
  
  // Start the service
  bmeService->start();

  // Start Temporary Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}


void loop() {
  if (deviceConnected) {
    show == 0;
    if ((millis() - lastTime) > timerDelay) {
      Serial.println("Start Reading...");
      // Read temperature as Celsius (the default)
      temp = bme.readTemperature();
      // Fahrenheit
      tempF = 1.8*temp +32;
      // Read humidity
      hum = bme.readHumidity();
  
      //Notify temperature reading from BME sensor
      #ifdef temperatureCelsius
        static char temperatureCTemp[6];
        dtostrf(temp, 6, 2, temperatureCTemp);
        //Set temperature Characteristic value and notify connected client
        bmeTemperatureCelsiusCharacteristics.setValue(temperatureCTemp);
        bmeTemperatureCelsiusCharacteristics.notify();
        Serial.print("Temperature Celsius: ");
        Serial.print(temp);
        Serial.print(" deg C");
      #else
        static char temperatureFTemp[6];
        dtostrf(tempF, 6, 2, temperatureFTemp);
        //Set temperature Characteristic value and notify connected client
        bmeTemperatureFahrenheitCharacteristics.setValue(temperatureFTemp);
        bmeTemperatureFahrenheitCharacteristics.notify();
        Serial.print("Temperature Fahrenheit: ");
        Serial.print(tempF);
        Serial.print(" ºF");
      #endif
      
      //Notify humidity reading from BME
      static char humidityTemp[6];
      dtostrf(hum, 6, 2, humidityTemp);
      //Set humidity Characteristic value and notify connected client
      bmeHumidityCharacteristics.setValue(humidityTemp);
      bmeHumidityCharacteristics.notify();   
      Serial.print(" - Humidity: ");
      Serial.print(hum);
      Serial.println(" %");
      
      lastTime = millis();
    }
  }
  
  
}
