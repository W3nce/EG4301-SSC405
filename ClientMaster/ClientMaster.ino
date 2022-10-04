/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/
#include <WiFi.h>
#include "BLEDevice.h"
#include "BLEAdvertisedDevice.h"
#include <Wire.h>
//#include <Adafruit_SSD1306.h>
//#include <Adafruit_GFX.h>
#define temperatureCelsius

#define bleServerName "BME280_ESP32_SadServerSlave"
#define bleServerName2 "BME280_ESP32_SadServerSlave2"

static BLEUUID bmeServiceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");
static BLEUUID bmeServiceUUID2("30880f64-4239-11ed-b878-0242ac120002");

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

static const int DeviceUsedCount = 1;
static boolean deviceConnected = false;

//Address of the peripheral device. Address will be found during scanning...
static BLEAddress* pServerAddress;

//Activate notify
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

//Variables to store temperature and humidity
char* temperatureChar;
char* humidityChar;

//Flags to check whether new temperature and humidity readings are available
boolean newTemperature = false;
boolean newHumidity = false;

//Connect to the BLE Server that has the name, Service, and Characteristics
// Should take in Server Address, and save charaterstitics after confirm exist and reg for notify

class ClientServerManager {
  static const int sizelimit = 5;
  int TotalClientServer = 0;
  BLEClient* ClientList[sizelimit];
  std::string ConnectedDevNameList[sizelimit];
  BLEAdvertisedDevice* ConnectedAdvDevList[sizelimit];
  BLEAddress* ServerAddressList[sizelimit];
  bool CharReadyList[sizelimit] = { false };
  bool TemperatureCharNotifyList[sizelimit] = { false };
  bool HumidityCharNotifyList[sizelimit] = { false };
  std::string TemperatureCharList[sizelimit];
  std::string HumidityCharList[sizelimit];

public:
  int getNumberofClientServer();
  bool contains(std::string AdvDevName);
  bool contains(BLEAdvertisedDevice* AdvDev);
  bool contains(BLEAddress* Server);
  void addDevice(BLEAdvertisedDevice* AdvDev);
  void connectToServer(BLEAdvertisedDevice* AdvDev);
  void tryConnect();
  bool check();
  void setTempChar(std::string Server, std::string Value);
  void setHumChar(std::string Server, std::string Value);
  void printReadings();
};

static ClientServerManager* MyClientServerManager = new ClientServerManager();

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

//When the BLE Server sends a new temperature reading with the notify property
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                      uint8_t* pData, size_t length, bool isNotify) {
  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  //store temperature value
  temperatureChar = (char*)pData;

  std::string TempString = temperatureChar;
  MyClientServerManager->setTempChar(ServerAddressStr,TempString);
}

//When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                   uint8_t* pData, size_t length, bool isNotify) {
  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();

  //store humidity value
  humidityChar = (char*)pData;
  std::string HumString = humidityChar;
  MyClientServerManager->setHumChar(ServerAddressStr,HumString);
}




int ClientServerManager::getNumberofClientServer() {
  return TotalClientServer;
}

bool ClientServerManager::contains(std::string AdvDevName) {  // Method/function defined inside the class
  if (AdvDevName == "") {
    return false;
  }
  for (int i = 0; i < sizelimit; i++) {
    if (ConnectedDevNameList[i] == AdvDevName) {
      return true;
    }
  }
  return false;
}

bool ClientServerManager::contains(BLEAdvertisedDevice* AdvDev) {  // Method/function defined inside the class
  if (!AdvDev->haveName()) {
    return false;
  }
  for (int i = 0; i < sizelimit; i++) {
    BLEAddress OtherServer = *ServerAddressList[i];
    if (AdvDev->getAddress().equals(OtherServer)) {
      return true;
    }
  }
  return false;
}

bool ClientServerManager::contains(BLEAddress* Server) {  // Method/function defined inside the class

  for (int i = 0; i < sizelimit; i++) {
    BLEAddress OtherServer = *ServerAddressList[i];
    if (Server->equals(OtherServer)) {
      return true;
    }
  }
  return false;
}

void ClientServerManager::addDevice(BLEAdvertisedDevice* AdvDev) {
  if (contains(AdvDev)) {
    Serial.println("Device Already Configured");
    return;
  }
  for (int i = 0; i < sizelimit; i++) {
    if (ConnectedAdvDevList[i] == nullptr) {
      TotalClientServer = TotalClientServer + 1;
      ConnectedDevNameList[i] = AdvDev->getName().c_str();
      ConnectedAdvDevList[i] = AdvDev;
      ServerAddressList[i] = new BLEAddress(AdvDev->getAddress().toString());
    }
    Serial.println("Server List Full");
    return;
  }
}

void ClientServerManager::connectToServer(BLEAdvertisedDevice* AdvDev) {
  Serial.printf("Attempting to connect: %s ", AdvDev->getName().c_str());
  BLEAddress* AdvServerAdress = new BLEAddress(AdvDev->getAddress().toString());
  Serial.printf("Server Address: %s \n", AdvServerAdress->toString().c_str());
  if (!contains(AdvDev)) {
    Serial.println("Device not configured");
    return;
  }
  for (int i = 0; i < sizelimit; i++) {
    if (AdvServerAdress->equals(*ServerAddressList[i])) {
      BLEClient* pClient = BLEDevice::createClient();

      pClient->setClientCallbacks(new MyClientCallback());
      pClient->connect(AdvDev);
      Serial.println(" - Connected to server");
      // Obtain a reference to the service we are after in the remote BLE server.
      BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
      if (pRemoteService == nullptr) {
        Serial.println("Failed to find our service UUID: ");
        Serial.println(bmeServiceUUID.toString().c_str());
        return;
      }
      // Obtain a reference to the characteristics in the service of the remote BLE server.
      BLERemoteCharacteristic* TempChar = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
      BLERemoteCharacteristic* HumChar = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

      if (TempChar == nullptr || HumChar == nullptr) {
        Serial.println("Failed to find our characteristic UUID");
        return;
      }
      Serial.println(" - Found our characteristics");

      //Assign callback functions for the Characteristics
      TempChar->registerForNotify(temperatureNotifyCallback);
      HumChar->registerForNotify(humidityNotifyCallback);

      //Activate the Notify property of each Characteristic
      TempChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      HumChar->getDescriptor(BLEUUID((uint16_t)0x2903))->writeValue((uint8_t*)notificationOn, 2, true);

      CharReadyList[i] = true;
      ClientList[i] = pClient;

      return;
    }
  }
  return;
}

void ClientServerManager::tryConnect() {
  for (int i = 0; i < sizelimit; i++) {
    connectToServer(ConnectedAdvDevList[i]);
  }
  return;
}

bool ClientServerManager::check() {
  if (DeviceUsedCount == TotalClientServer) {
    return true;
    Serial.printf("All Server Connected: %d / %d Devices \n", TotalClientServer, DeviceUsedCount);
  } else {
    Serial.printf("Not all Server Connected: %d / %d Devices \n", TotalClientServer, DeviceUsedCount);
    return false;
  }
}

void ClientServerManager::setTempChar(std::string Server, std::string Value) {  // Method/function defined inside the class

  for (int i = 0; i < sizelimit; i++) {
    std::string SavedServer = ServerAddressList[i]->toString();
    if (SavedServer.compare(Server)) {
      Serial.printf("Humidity Notified from %s \n", SavedServer);
      TemperatureCharList[i] = Value;
      TemperatureCharNotifyList[i] = true;
    }
  }
}

void ClientServerManager::setHumChar(std::string Server, std::string Value) {  // Method/function defined inside the class

  for (int i = 0; i < sizelimit; i++) {
    std::string SavedServer = ServerAddressList[i]->toString();
    if (SavedServer.compare(Server)) {
      Serial.printf("Temperature Notified from %s \n", SavedServer);
      HumidityCharList[i] = Value;
      HumidityCharNotifyList[i] = true;
    }
  }
}

//function that prints the latest sensor readings in the OLED display
void ClientServerManager::printReadings() {
  for (int i = 0; i < sizelimit; i++) {
    std::string SavedServer = ServerAddressList[i]->toString();
    if (CharReadyList[i] && TemperatureCharNotifyList[i] && HumidityCharNotifyList[i]) {

      Serial.printf("BME Device: %s - Server Address: %s \n", ConnectedDevNameList[i], SavedServer);
      Serial.printf("Readings: Temperature = %s ºC - ", temperatureChar);
      Serial.printf("Humidity = %s ", humidityChar);
      Serial.println("%");
      TemperatureCharNotifyList[i] = false;
      HumidityCharNotifyList[i] = false;
    }
  }
}


//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.println("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    if (advertisedDevice.haveName() && !MyClientServerManager->contains(&advertisedDevice)) {
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(bmeServiceUUID)) {
        //advertisedDevice.getScan()->stop(); //Scan can be stopped, we found what we are looking for
        //create pointer of advdev addDevice(BLEAdvertisedDevice* AdvDev)
        MyClientServerManager->addDevice(&advertisedDevice);

        Serial.printf("Total Device found and Saved: %d \n", MyClientServerManager->getNumberofClientServer());

      } else {
        Serial.println("UUID Unmatch! Device not Connected.");
      }
    } else if (!advertisedDevice.haveName()) {
      Serial.println("Device Name");
    } else {
      Serial.println("Device already Connected: ");
      // Continue Pairing another Device
    }
  }
};


void setup() {
  //Start serial communication
  Serial.begin(115200);
  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  Serial.println("Starting Arduino BLE Client application...");

  //Init BLE device
  BLEDevice::init("");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);

  if (MyClientServerManager->check()) {
    MyClientServerManager->tryConnect();
  } else {
    int waittime = 10;
    pBLEScan->start(waittime);
    Serial.print("Attempting Rescan");
    for (int i = 0; i < waittime; i++) {
      
    Serial.print(".");
    }
    Serial.println("");
    bool check = MyClientServerManager->check();
    MyClientServerManager->tryConnect();
  }
}

void loop() {
  MyClientServerManager->printReadings();
  delay(1000);  // Delay a second between loops.
}
