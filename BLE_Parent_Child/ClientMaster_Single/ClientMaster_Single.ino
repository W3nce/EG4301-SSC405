/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
//BLE Device Framework for creation of Parent
#include "BLEDevice.h"
#include <Wire.h>

//Declaring the pins used for LED
int led1 = D10;
int led2 = D9;

//Default Temperature is in Celsius
#define temperatureCelsius

//BLE Server name (the other ESP32 name running the server sketch)
#define bleServerName "Child_1"

/* UUID's of the service, characteristic that we want to read*/
static BLEUUID bmeServiceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");

// BLE Characteristics
#ifdef temperatureCelsius
  //Temperature Celsius Characteristic
  static BLEUUID temperatureCharacteristicUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");
#else
  //Temperature Fahrenheit Characteristic
  static BLEUUID temperatureCharacteristicUUID("f78ebbff-c8b7-4107-93de-889a6a06d408");
#endif

// Humidity Characteristic
static BLEUUID humidityCharacteristicUUID("ca73b3ba-39f6-4ab3-91ae-186dc9577d99");

//Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean connected = false;

//Address of the advertised Server found during scanning...
static BLEAddress *pServerAddress;
//Reference to the advertised Server found during scanning...
static BLEAdvertisedDevice *pAdvertisedDevice;
 
//Characteristicd that we want to read
static BLERemoteCharacteristic* temperatureCharacteristic;
static BLERemoteCharacteristic* humidityCharacteristic;

//Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

//Variables to store temperature and humidity
//static char* temperatureChar;
//static char* humidityChar;
static  std::string temperatureCTemp;
static  std::string humidityTemp;


//Flags to check whether new temperature and humidity readings are available
boolean newTemperature = false;
boolean newHumidity = false;

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("ClientCallBack: onConnect");
    digitalWrite(led1, HIGH);
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("ClientCallBack: onDisconnect");
    digitalWrite(led1, LOW);
  }
};

//Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAdvertisedDevice * pDevice) {
   BLEClient* pClient = BLEDevice::createClient();
 
  // Connect to the remove BLE Server.
  
  //pClient->connect(pAddress);
  pClient->setClientCallbacks(new MyClientCallback());

  pClient->connect(pDevice);
  Serial.println(" - Connected to server");
 
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    return (false);
  }
 
  // Obtain a reference to the characteristics in the service of the remote BLE server.
  temperatureCharacteristic = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
  humidityCharacteristic = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

  if (temperatureCharacteristic == nullptr || humidityCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID");
    return false;
  }
  Serial.println(" - Found our characteristics");
 
  //Assign callback functions for the Characteristics
  temperatureCharacteristic->registerForNotify(temperatureNotifyCallback);
  humidityCharacteristic->registerForNotify(humidityNotifyCallback);
  return true;
}

//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) { //Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop(); //Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); //Address of advertiser is the one we need
      pAdvertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true; //Set indicator, stating that we are ready to connect
      Serial.println("Device found. Connecting!");
    }
  }
};
 
//When the BLE Server sends a new temperature reading with the notify property
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                        uint8_t* pData, size_t length, bool isNotify) {
  //store temperature value
  char temperatureChar[6];
  strcpy(temperatureChar,(char*)pData);
  temperatureChar[6] = '\0';
  //char * temperatureChar = (char*)pData;
  temperatureCTemp = temperatureChar;
  newTemperature = true;  
  Serial.print("Temp Notify: ");
  //Serial.printf(temperatureChar);
  Serial.printf("%s",temperatureCTemp.c_str());
  Serial.println("");
}

//When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                    uint8_t* pData, size_t length, bool isNotify) {
  //store humidity value
  char humidityChar[6];
  strcpy(humidityChar,(char*)pData);
  humidityChar[6] = '\0';
  //humidityChar = (char*)pData;
  humidityTemp = humidityChar;
  newHumidity = true;
  Serial.print("Hum Notify: ");
  //Serial.printf(humidityChar);
  Serial.printf("%s",humidityTemp.c_str());
  Serial.println("");
}

//function that prints the latest sensor readings in the OLED display
void printReadings(){
  Serial.print("Temperature: ");
  Serial.printf("%s",temperatureCTemp.c_str());
  #ifdef temperatureCelsius
    //Temperature Celsius
    Serial.print("C -");
  #else
    //Temperature Fahrenheit
    Serial.print("F");
  #endif
  Serial.print(" Humidity: ");
  Serial.printf("%s",humidityTemp.c_str()); 
  Serial.println("% \n");
}

void setup() {
  
  //Start serial communication
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  
  //Initialise LED1 and LED2
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  
  //Init BLE device
  BLEDevice::init("");
 
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(pAdvertisedDevice)) {
      Serial.println("We are now connected to the BLE Server.");
      //Activate the Notify property of each Characteristic
      temperatureCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
    }
    doConnect = false;
  }
  //if new temperature readings are available, print in the OLED
  if (newTemperature && newHumidity){
    digitalWrite(led2, HIGH);
    newTemperature = false;
    newHumidity = false;
    printReadings();
    delay(500);
    digitalWrite(led2, LOW);
  }
  delay(1000); // Delay a second between loops.
}