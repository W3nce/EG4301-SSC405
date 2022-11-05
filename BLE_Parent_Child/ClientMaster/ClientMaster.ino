#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "BLEDevice.h"
#include "BLEAdvertisedDevice.h"
#include <Wire.h>
#include <cstring>
//#include <ArduinoTrace.h>

// Settings Needed
static bool TurnOnScanResults = false;

// Replace with your network credentials
const char* ssid = "Terence";
const char* password = "qwertyios";

// Libraries to get time from NTP Server
#include <NTPClient.h>
#include <WiFiUdp.h>

//To calculate leap year
#define LEAP_YEAR(Y) ((Y > 0) && !(Y % 4) && ((Y % 100) || !(Y % 400)))

//Declaring the pins used for LED
const int ledPin1 = D10; 
const int ledPin2 = D9; 

//Default Temperature is in Celsius
#define temperatureCelsius

// Timer variables
unsigned long lastTime = 0;
//Logging Checking Interval
unsigned long loggingInterval = 10000;

//BLE Server name (the other ESP32 name running the server sketch)
std::string bleServerNameCheck = "Child_1";

//Total Number of Children to be connected
static const int DeviceUsedCount = 1;

/* UUID's of the service, characteristic that we want to read*/
static BLEUUID bmeServiceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");

#ifdef temperatureCelsius
//Temperature Celsius Characteristic
static BLEUUID temperatureCharacteristicUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");
#else
//Temperature Fahrenheit Characteristic
static BLEUUID temperatureCharacteristicUUID("f78ebbff-c8b7-4107-93de-889a6a06d408");
#endif

// Humidity Characteristic
static BLEUUID humidityCharacteristicUUID("ca73b3ba-39f6-4ab3-91ae-186dc9577d99");

//Scan Object Pointer
static BLEScan* pBLEScan;

//Activate notify
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

//returns Boolean String
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
  String DateTimeList[sizelimit];

public:
  int getNumberofClientServer();
  bool contains(std::string ServerStr);
  bool contains(BLEAddress* Server);
  bool contains(BLEAdvertisedDevice* AdvDev);
  int contains(BLEClient* client);
  void addDevice(BLEAdvertisedDevice* AdvDev);
  void removeDevice(BLEClient* client);
  void connectToServer(BLEAdvertisedDevice* AdvDev, BLEAddress* AdvServerAdress);
  void tryConnect();
  bool check();
  void setTempChar(std::string Server, std::string Value, String DateTimeString);
  void setHumChar(std::string Server, std::string Value, String DateTimeString);
  void sendReadings();
};

static ClientServerManager* MyClientServerManager = new ClientServerManager();

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("ClientCallBack: onConnect");
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("ClientCallBack: onDisconnect");
    MyClientServerManager->removeDevice(pclient);
  }
};

String getFormattedDate() {
  unsigned long rawTime = timeClient.getEpochTime() / 86400L;  // in days
  unsigned long days = 0, year = 1970;
  uint8_t month;
  static const uint8_t monthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  while ((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
    year++;
  rawTime -= days - (LEAP_YEAR(year) ? 366 : 365);  // now it is days in this year, starting at 0
  days = 0;
  for (month = 0; month < 12; month++) {
    uint8_t monthLength;
    if (month == 1) {  // february
      monthLength = LEAP_YEAR(year) ? 29 : 28;
    } else {
      monthLength = monthDays[month];
    }
    if (rawTime < monthLength) break;
    rawTime -= monthLength;
  }
  String monthStr = ++month < 10 ? "0" + String(month) : String(month);      // jan is month 1
  String dayStr = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime);  // day of month
  return String(year) + "-" + monthStr + "-" + dayStr + "T" + timeClient.getFormattedTime() + "Z";
}
// Function to get date and time from NTPClient
String getTimeStamp() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  String formattedDate = getFormattedDate();

  // Extract date
  int splitT = formattedDate.indexOf("T");
  String dayStamp = formattedDate.substring(0, splitT);
  //Serial.println(dayStamp);
  // Extract time
  String timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  //Serial.println(timeStamp);
}
//When the BLE Server sends a new temperature reading with the notify property
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                      uint8_t* pData, size_t length, bool isNotify) {
  timeClient.update();
  Serial.print("[[Notification (TEMP)]] @ ");
  String DateTimeString = getFormattedDate();
  Serial.println(DateTimeString);

  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();
  Serial.printf("Server (%s) Notified Temperature: ", ServerAddressStr.c_str());
  char* temperatureChar = (char*)pData;
  temperatureChar[6] = '\0';
  std::string TempString = temperatureChar;
  Serial.printf("%s C \n", TempString.c_str());
  Serial.print("-> Call setTempChar: ");
  MyClientServerManager->setTempChar(ServerAddressStr, TempString, DateTimeString);
  Serial.println("");
}
//When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                   uint8_t* pData, size_t length, bool isNotify) {
  timeClient.update();
  Serial.print("[[Notification (HUM)]] @ ");
  String DateTimeString = getFormattedDate();
  Serial.println(DateTimeString);

  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();

  Serial.printf("Server (%s) Notified Humidity: ", ServerAddressStr.c_str());
  char* humidityChar = (char*)pData;
  humidityChar[6] = '\0';
  std::string HumString = humidityChar;
  Serial.printf("Humidity: %s %% \n", HumString.c_str());
  Serial.print("-> Call setHumChar: ");
  MyClientServerManager->setHumChar(ServerAddressStr, HumString, DateTimeString);
  Serial.println("");
}
int ClientServerManager::getNumberofClientServer() {
  return TotalClientServer;
}
bool ClientServerManager::contains(std::string ServerStr) {  // Method/function defined inside the class
  Serial.print("Contains ServerStr: @BLEAddressName ");
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
int ClientServerManager::contains(BLEClient* client) {  // Method/function defined inside the class
  Serial.print("Contains Client: @BLEClient ");
  for (int i = 0; i < sizelimit; i++) {
    if (ClientList[i] == nullptr) {
      Serial.println("false (Not Exist)");
      return -1;
    }
    BLEClient otherCLient = *ClientList[i];
    std::string otherClientStr = otherCLient.toString();
    if (otherClientStr.compare(client->toString()) == 0) {

      Serial.println("true (Found)");
      return i;
    }
  }
  return -1;
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
void ClientServerManager::removeDevice(BLEClient* client) {
  int index = contains(client);
  if (index==-1) {
    Serial.println("removeClient: Cannot find Client");
    return;
  } else {
    TotalClientServer = TotalClientServer - 1;
    ConnectedDevNameList[index] = "";
    ConnectedAdvDevList[index] = NULL;
    ServerAddressList[index] = NULL;
    CharReadyList[index] = false;
    TemperatureCharNotifyList[index] = false;
    HumidityCharNotifyList[index] = false;
    ClientList[index]->disconnect();
    ClientList[index] = NULL;

    Serial.println("removeClient: Successfully Removed");
    Serial.printf("Current Server Connected: %d / %d Devices \n", TotalClientServer, DeviceUsedCount);
    Serial.println("");
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
      Serial.println("Not Connected Proceed -> \n");
      connectToServer(ConnectedAdvDevList[i], ServerAddressList[i]);
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
void ClientServerManager::setTempChar(std::string Server, std::string Value, String DateTimeString) {  // Method/function defined inside the class
  Serial.printf("Setting Temp Char for Server(%s): \n", Server.c_str());
  Serial.print("First Server: ");
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("No Server to set!");
      return;
    }
    std::string SavedServer = ServerAddressList[i]->toString();
    Serial.printf("Checking Target Address (%s) : ", SavedServer.c_str());
    if (strcmp(SavedServer.c_str(), Server.c_str()) == 0) {
      TemperatureCharList[i] = Value;
      TemperatureCharNotifyList[i] = true;
      DateTimeList[i] = DateTimeString;
      Serial.println("Success: TempChar Set for Print");
      return;
    } else {
      Serial.println("Failed: Server Address Unmatch");
    }
    Serial.print("Next Server: ");
  }
  return;
}
void ClientServerManager::setHumChar(std::string Server, std::string Value, String DateTimeString) {  // Method/function defined inside the class

  Serial.printf("Setting Hum Char for Server(%s): \n", Server.c_str());
  Serial.print("First Server: ");
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("No Server to set!");
      return;
    }
    std::string SavedServer = ServerAddressList[i]->toString();
    Serial.printf("Checking Target Address (%s) : ", SavedServer.c_str());
    if (strcmp(SavedServer.c_str(), Server.c_str()) == 0) {
      HumidityCharList[i] = Value;
      HumidityCharNotifyList[i] = true;
      DateTimeList[i] = DateTimeString;
      Serial.println("Success: HumChar Set for Print");
      return;
    } else {
      Serial.println("Failed: Server Address Unmatch");
    }
    Serial.print("Next Server: ");
  }
  return;
}
//function that prints the latest sensor readings in the OLED display
void ClientServerManager::sendReadings() {
  Serial.println("[[SerialPrint]]");
  Serial.print("SendReadings: ");
  for (int i = 0; i < sizelimit; i++) {
    if (ServerAddressList[i] == nullptr) {
      Serial.println("No More Server to print!\n");
      return;
    }
    std::string SavedServer = ServerAddressList[i]->toString();
    if (CharReadyList[i] && TemperatureCharNotifyList[i] && HumidityCharNotifyList[i]) {

      Serial.printf("BME Device: %s - Server Address: %s \n", ConnectedDevNameList[i].c_str(), SavedServer.c_str());
      Serial.printf("Readings: Temperature = %s C - ", TemperatureCharList[i].c_str());
      Serial.printf("Humidity = %s %% - ", HumidityCharList[i].c_str());
      Serial.printf("Date Time = %s ", DateTimeList[i].c_str());
      Serial.println("\n");
      TemperatureCharNotifyList[i] = false;
      HumidityCharNotifyList[i] = false;
    }
  }
  return;
}
void switchprintf(const char* main, const char* format, bool bswitch) {
  if (bswitch) {
    Serial.printf(main, format);
  }
}
void switchprint(const char* main, bool bswitch) {
  if (bswitch) {
    Serial.printf(main);
  }
}
void switchprintln(const char* main, bool bswitch) {
  if (bswitch) {
    Serial.println(main);
  }
}
//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveName() && !MyClientServerManager->contains(&advertisedDevice)) {
      switchprintln("[[SCAN]]", TurnOnScanResults);
      switchprintln("BLE Advertised Device found: ", TurnOnScanResults);
      switchprintln(advertisedDevice.toString().c_str(), TurnOnScanResults);
      if (advertisedDevice.getName() == bleServerNameCheck && advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(bmeServiceUUID)) {
        //Scan can be stopped, we found what we are looking for
        //create pointer of advdev addDevice(BLEAdvertisedDevice* AdvDev)
        BLEAdvertisedDevice* NewDev = new BLEAdvertisedDevice(advertisedDevice);
        MyClientServerManager->addDevice(NewDev);

        Serial.printf("Total Device found and Saved: %d \n", MyClientServerManager->getNumberofClientServer());
        advertisedDevice.getScan()->stop();
      } else {
        switchprintln("UUID/Name Unmatch! Device not Connected.", TurnOnScanResults);
      }
      switchprintln("", TurnOnScanResults);
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
void setup() {
  //Initialise LED1 and LED2, Turn on LED1
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin1, HIGH);

  //Start serial communication
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting Arduino BLE Client application...");  
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  //Starts WiFi Connection
    WiFi.mode(WIFI_STA);  
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  } 
  Serial.println(" connected");

  //Starts NTP Client
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  //Init BLE device
  BLEDevice::init("");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}
void loop() {
  digitalWrite(ledPin2, HIGH);
  int waittime = 5;
  if ((millis() - lastTime) > loggingInterval) {
    digitalWrite(ledPin2, HIGH);
    if (MyClientServerManager->getNumberofClientServer() == 0) {
      rescan(pBLEScan, waittime);
      return;
    }
    if (!MyClientServerManager->check()) {
      rescan(pBLEScan, waittime);
    }
    MyClientServerManager->tryConnect();

    MyClientServerManager->sendReadings();

    lastTime = millis();
    delay(500);
    
    digitalWrite(ledPin2, LOW);
  }
  delay(3000);  // Delay a second between loops.
}