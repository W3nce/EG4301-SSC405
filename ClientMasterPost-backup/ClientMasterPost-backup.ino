/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "BLEDevice.h"
#include "BLEAdvertisedDevice.h"
#include <Wire.h>
#include <WiFiMulti.h>
#include <cstring>
//#include <ArduinoTrace.h>
#define temperatureCelsius

//#define bleServerName "BME280_ESP32_SadServerSlave"

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


static bool TurnOnScanResults = false;

static const int DeviceUsedCount = 1;

//Scan Object Pointer
static BLEScan* pBLEScan;

//Activate notify
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

std::string boolString(bool b) {
  if (b) {
    return "true";
  } else {
    return "false";
  }
}

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
  bool contains(std::string ServerStr);
  bool contains(BLEAddress* Server);
  bool contains(BLEAdvertisedDevice* AdvDev);
  void addDevice(BLEAdvertisedDevice* AdvDev);
  void connectToServer(BLEAdvertisedDevice* AdvDev,BLEAddress* AdvServerAdress);
  void tryConnect();
  bool check();
  void setTempChar(std::string Server, std::string Value);
  void setHumChar(std::string Server, std::string Value);
  void printReadings();
};

static ClientServerManager* MyClientServerManager = new ClientServerManager();

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("ClientCallBack: onConnect");
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("ClientCallBack: onDisconnect");
  }
};

//When the BLE Server sends a new temperature reading with the notify property
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                      uint8_t* pData, size_t length, bool isNotify) {  
  Serial.println("[[Notification (TEMP)]]");
  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  Serial.printf("Server (%s) Notified Temperature: ",ServerAddressStr.c_str());
  char*temperatureChar = (char*)pData;
  std::string TempString = temperatureChar;
  Serial.printf("Temperature: %s C \n",TempString.c_str());
  Serial.print("-> Call setTempChar: ");
  MyClientServerManager->setTempChar(ServerAddressStr, TempString);
  Serial.println("");
}

//When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                   uint8_t* pData, size_t length, bool isNotify) {
  Serial.println("[[Notification (HUM)]]");
  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();

  Serial.printf("Server (%s) Notified Humidity: ",ServerAddressStr.c_str());
  char*humidityChar = (char*)pData;
  std::string HumString = humidityChar;
  Serial.printf("Humidity: %s %% \n",HumString.c_str());
  Serial.print("-> Call setHumChar: ");
  MyClientServerManager->setHumChar(ServerAddressStr, HumString);
  Serial.println("");
  }




int ClientServerManager::getNumberofClientServer() {
  return TotalClientServer;
}

bool ClientServerManager::contains(std::string ServerStr) {  // Method/function defined inside the class
  Serial.print("Contains ServerStr: @BLEAddress ");
  if (ServerStr == "") {
    Serial.println("false (Address NIL)");
    return false;
  }
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("false (Not Exist)");
      return false;
    }
    if (ServerAddressList[i]->toString() == ServerStr) {
      Serial.println("true (Found)");
      return true;
    }
  }
  return false;
}


bool ClientServerManager::contains(BLEAddress* Server) {  // Method/function defined inside the class
  Serial.print("Contains Server: @BLEAddress ");
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("false (Not Exist)");
      return false;
    }
    BLEAddress OtherServer = *ServerAddressList[i];
    if (Server->equals(OtherServer)) {
      Serial.println("true (Found)");
      return true;
    }
  }
  return false;
}

bool ClientServerManager::contains(BLEAdvertisedDevice* AdvDev) {  // Method/function defined inside the class

  Serial.print("Contains AdvDev: @BLEAdvDevice ");
  if (!AdvDev->haveName()) {
    Serial.println("false (No Name)");
    return false;
  }
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("false (Not Exist)");
      return false;
    }
    BLEAddress OtherServer = *ServerAddressList[i];

    if (AdvDev->getAddress().equals(OtherServer)) {
      Serial.println("true (Found)");
      return true;
    }
  }
  return false;
}

void ClientServerManager::addDevice(BLEAdvertisedDevice* AdvDev) {
  if (contains(AdvDev)) {
    Serial.println("addDevice: Device Already Configured");
    return;
  }
  for (int i = 0; i < sizelimit; i++) {
    if (ConnectedAdvDevList[i] == nullptr) {
      TotalClientServer = TotalClientServer + 1;
      ConnectedDevNameList[i] = AdvDev->getName().c_str();
      ConnectedAdvDevList[i] = AdvDev;
      ServerAddressList[i] = new BLEAddress(*AdvDev->getAddress().getNative());
      return;
    }
    Serial.println("addDevice: Server List Full");
    return;
  }
}

void ClientServerManager::connectToServer(BLEAdvertisedDevice* AdvDev, BLEAddress* AdvServerAdress) {

  Serial.println("[[ServerConnect]]");  

  Serial.printf("Attempting to connect: %s ", AdvDev->getName().c_str());
  Serial.printf("Server Address: %s \n", AdvServerAdress->toString().c_str());

  if (!contains(AdvServerAdress)) {
    Serial.println("Device not configured");
    return;
  }
  
    Serial.print("ConnectToServer: ");
  for (int i = 0; i < sizelimit; i++) {

    if (ServerAddressList[i] == nullptr) {
      Serial.println("Cannot find the Server to connect (Null Pointer)");
      return;
    }
    if (AdvServerAdress->equals(*ServerAddressList[i])) {
      BLEClient* pClient = BLEDevice::createClient();
      pClient->setClientCallbacks(new MyClientCallback());      
      pClient->connect(AdvDev); 
      Serial.println("Connected to server");

      // Obtain a reference to the service we are after in the remote BLE server.      
      Serial.print("Getting Remote Service: ");

      BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
      Serial.println("Tried to get remote Service");
      if (pRemoteService == nullptr) {
        Serial.println("Failed to find our service UUID: ");
        Serial.println(bmeServiceUUID.toString().c_str());
        return;
      }      
      Serial.println("Found Service UUID");
      
      Serial.print("Setting Characteristics: ");
      // Obtain a reference to the characteristics in the service of the remote BLE server.
      BLERemoteCharacteristic* TempChar = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
      BLERemoteCharacteristic* HumChar = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

      if (TempChar == nullptr || HumChar == nullptr) {
        Serial.println("Failed to find our characteristic UUID\n");
        pClient->disconnect();
        return;
      }
      Serial.println("Found our characteristics");

      //Assign callback functions for the Characteristics
      TempChar->registerForNotify(temperatureNotifyCallback);
      HumChar->registerForNotify(humidityNotifyCallback);

      //Activate the Notify property of each Characteristic
      TempChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      HumChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);

      CharReadyList[i] = true;
      ClientList[i] = pClient;
      Serial.println("Connect Success!");

      return;
    }
    Serial.println("");
  }
  Serial.println("Cannot find the Server to connect (End of Array)\n");
  return;
}

void ClientServerManager::tryConnect() {
  Serial.println("[[ConnectALL]]");
  Serial.print("TryConnect: ");
  if (TotalClientServer == 0) {
    Serial.println("No Server to connect! (Empty Array)");
    return;
  }
   
    Serial.print("First Server: ");
  for (int i = 0; i < sizelimit; i++) { 
    if (ServerAddressList[i] == nullptr) {
      Serial.println("No Server to connect! (Null Pointer)\n");
      return;
    }    
    Serial.print("Proceed -> ");
    
    Serial.print("Check Connected: ");
    if (!CharReadyList[i]) {
      Serial.println("Not Connected proceed -> \n");
      connectToServer(ConnectedAdvDevList[i],ServerAddressList[i]);    
      Serial.println("");        
      Serial.print("Next Server: ");   
    }    
  }
  Serial.println("No Server to connect! (End of Array)");
  return;
}

bool ClientServerManager::check() {
  Serial.println("[[ServerCheck]]");  
  Serial.print("ServerCheck: ");
  if (DeviceUsedCount == TotalClientServer) {
    Serial.printf("All Server Connected: %d / %d Devices \n", TotalClientServer, DeviceUsedCount);
    Serial.println("");
    return true;
  } else {
    Serial.printf("Not all Server Connected: %d / %d Devices \n", TotalClientServer, DeviceUsedCount);
    Serial.println("");
    return false;
  }
}

void ClientServerManager::setTempChar(std::string Server, std::string Value) {  // Method/function defined inside the class
  Serial.printf("Setting Temp Char for Server(%s): \n",Server.c_str());
  Serial.print("First Server: ");
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("No Server to set!");
      return;
    }    
    std::string SavedServer = ServerAddressList[i]->toString();
    Serial.printf("Checking Target Address (%s) : ",SavedServer.c_str());
    if (strcmp(SavedServer.c_str(), Server.c_str())==0) {
      TemperatureCharList[i] = Value;
      TemperatureCharNotifyList[i] = true;      
      Serial.println("Success: TempChar Set for Print");
      return;
    }    else{
    Serial.println("Failed: Server Address Unmatch");
    }
    Serial.print("Next Server: ");
  }
  return;
}

void ClientServerManager::setHumChar(std::string Server, std::string Value) {  // Method/function defined inside the class

  Serial.printf("Setting Hum Char for Server(%s): \n",Server.c_str());
  Serial.print("First Server: ");
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("No Server to set!");
      return;
    }
    std::string SavedServer = ServerAddressList[i]->toString();
    Serial.printf("Checking Target Address (%s) : ",SavedServer.c_str());
    if (strcmp(SavedServer.c_str(), Server.c_str())==0) {
      HumidityCharList[i] = Value;
      HumidityCharNotifyList[i] = true;    
      Serial.println("Success: HumChar Set for Print");
      return;
    }    
    else{
    Serial.println("Failed: Server Address Unmatch");
    }
    Serial.print("Next Server: ");
  }
  return;
}

//function that prints the latest sensor readings in the OLED display
void ClientServerManager::printReadings() {
  Serial.println("[[SerialPrint]]");
  Serial.print("PrintReadings: ");
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("No More Server to print!\n");
      return;
    }
    std::string SavedServer = ServerAddressList[i]->toString();
    if (CharReadyList[i] && TemperatureCharNotifyList[i] && HumidityCharNotifyList[i]) {

      Serial.printf("BME Device: %s - Server Address: %s \n", ConnectedDevNameList[i].c_str(), SavedServer.c_str());
      Serial.printf("Readings: Temperature = %s C - ", TemperatureCharList[i].c_str());
      Serial.printf("Humidity = %s ", HumidityCharList[i].c_str());
      Serial.println("%\n");
      TemperatureCharNotifyList[i] = false;
      HumidityCharNotifyList[i] = false;
      String tempData = TemperatureCharList[i].c_str();
      String humData = HumidityCharList[i].c_str();
      WiFiClientSecure *client = new WiFiClientSecure;
  //String hostserver = "https://192.168.18.6:6802/postEnv"; local server
  String hostserver = "https://sussylogger.fly.dev/postEnv"; //web server
  if(client) {
//    client -> setCACert(rootCACertificate);
    client -> setInsecure(); //comment to test secure connection
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, hostserver)) {  // HTTPS
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {

            String httpRequestData = "temperature="+tempData+"&humidity="+humData;
            https.addHeader("Content-Type", "application/x-www-form-urlencoded");
            int httpResponseCode =  https.POST(httpRequestData);
            
            Serial.print("HTTP POST Response code: ");
            Serial.println(httpResponseCode);
          }

          
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }

      // End extra scoping block
    }
  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
    }
  }
  return;
}

void switchprintf(const char* main, const char* format,bool bswitch){
  if (bswitch){
    Serial.printf(main,format);
  }
}
void switchprint(const char* main,bool bswitch){
  if (bswitch){
    Serial.printf(main);
  }
}
void switchprintln(const char* main,bool bswitch){
  if (bswitch){
    Serial.println(main);
  }
}


//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveName() && !MyClientServerManager->contains(&advertisedDevice)) {  
        switchprintln("[[SCAN]]",TurnOnScanResults);    
        switchprintln("BLE Advertised Device found: ",TurnOnScanResults);
        switchprintln(advertisedDevice.toString().c_str(),TurnOnScanResults);
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(bmeServiceUUID)) {
         //Scan can be stopped, we found what we are looking for
        //create pointer of advdev addDevice(BLEAdvertisedDevice* AdvDev)
        BLEAdvertisedDevice * NewDev = new BLEAdvertisedDevice(advertisedDevice);
        MyClientServerManager->addDevice(NewDev);

        Serial.printf("Total Device found and Saved: %d \n", MyClientServerManager->getNumberofClientServer());
        advertisedDevice.getScan()->stop();
      } else {
        switchprintln("UUID/Name Unmatch! Device not Connected.",TurnOnScanResults);
      }
      switchprintln("",TurnOnScanResults);
    }
    /*else if (!advertisedDevice.haveName()) {
      Serial.println("No Device Name");
    } else {
      Serial.println("Device already Connected: ");
      // Continue Pairing another Device
    }*/
  }
};

void rescan(BLEScan* pBLEScan, int waittime) {
  pBLEScan->start(waittime, false);
  Serial.println("Attempting Rescan......");
  return;
}
WiFiMulti WiFiMulti;
void setup() {
  //Start serial communication
  Serial.begin(115200);
  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  Serial.println("Starting Arduino BLE Client application...");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("Mister Chua", "esp32sucksdick");// Change to reflect hotspot/home network

  // wait for WiFi connection
  Serial.print("Waiting for WiFi to connect...");
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    Serial.print(".");
  }
  Serial.println(" connected");
  //Init BLE device
  BLEDevice::init("");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}

void loop() {
  int waittime = 5;
  if (MyClientServerManager->getNumberofClientServer() == 0) {
    rescan(pBLEScan, waittime);
    return;
  } 
  if (!MyClientServerManager->check()) {
    rescan(pBLEScan, waittime);
  }
  MyClientServerManager->tryConnect();

  MyClientServerManager->printReadings();
  delay(10000);  // Delay a second between loops.
}
