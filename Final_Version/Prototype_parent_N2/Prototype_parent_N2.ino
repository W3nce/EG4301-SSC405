
#include "Prototype_parent_N2.h"
#include "UARTSerial_utils.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "SD_utils.h"
#include <cstdlib>
#include <ctime>

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

#define CurrShippingID      5
#define HRS_TO_SLEEP        8

//Activate notify
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

bool connected = false;

//Scan Object Pointer
static BLEScan* pBLEScan;

static ClientServerManager * MyClientServerManager = new ClientServerManager();
RTC_DATA_ATTR static bool ClientMasterInit = false;
RTC_DATA_ATTR static bool SDInit = false;
RTC_DATA_ATTR static int DeepSleepHrsCount = 0;
RTC_DATA_ATTR static int LastYr = 0;
RTC_DATA_ATTR static int LastMth = 0;
RTC_DATA_ATTR static int LastDay = 0;
RTC_DATA_ATTR static int LastHr = 0;
RTC_DATA_ATTR static int LastMin = 0;
RTC_DATA_ATTR static int LastSec = 0;
RTC_DATA_ATTR static int LastTZ = 0;

// counts the total number of readings taken for indexing
RTC_DATA_ATTR static int datacount = 0;
// index of last sent data
RTC_DATA_ATTR static int dataindex_last = 0;

RTC_DATA_ATTR static char add1[ServerAddressLength + 1] = "00:00:00:00:00:00";
RTC_DATA_ATTR static char add2[ServerAddressLength + 1] = "00:00:00:00:00:00";
RTC_DATA_ATTR static char add3[ServerAddressLength + 1] = "00:00:00:00:00:00";
RTC_DATA_ATTR static char * ServerAddressArray[sizelimit_def] = { add1, add2, add3};

int LED_arr[] = {LED_PIN_MAIN,LED_PIN_GREEN_1,LED_PIN_GREEN_2,LED_PIN_GREEN_3};

//Allocate for storing current date and timec bad location
static String currDateTimePOST = "1970-01-01:00:00%2B00:00";
static String currDateTimeCSV = "1970-01-01:00:00+00:00";
static float CurrLat = 0.00;
static float CurrLong = 0.00;
static float CurrTemp_Cloud;
static float CurrHum_Cloud;

//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {  

    const char * ServerFound = advertisedDevice.getName().c_str();

    if (advertisedDevice.haveName() 
    && advertisedDevice.haveServiceUUID() 
    && advertisedDevice.getServiceUUID().equals(bmeServiceUUID) 
    && in_ServerAddressArray(ServerFound)
    ){ 
      // advertisedDevice.getAddress().toString().c_str()

      //Check if the name of the advertiser matches
      Serial.println("@AdvDevCallbacks: Found BLE Advertised Device ");  

      if(ClientMasterInit 
      ){
        Serial.println("\t>> Exisiting Device found ");
        BLEAdvertisedDevice* NewDev = new BLEAdvertisedDevice(advertisedDevice);

        Serial.println("\t>> Adding Device");      
        connected = MyClientServerManager->addDevice(NewDev);
        Debugln(" ", TurnOnScanResults);    
      }

      // && !in_ServerAddressArray(advertisedDevice.getAddress().toString().c_str())
      else if (!ClientMasterInit 
      && (MyClientServerManager->contains(&advertisedDevice) == -1) 
      ){
        advertisedDevice.getScan()->stop();
        Serial.println("\t>> Initial Device found ");
        BLEAdvertisedDevice* NewDev = new BLEAdvertisedDevice(advertisedDevice);

        Serial.println("\t>> Adding Device");
        connected = MyClientServerManager->addDevice(NewDev);
        Debugln(" ", TurnOnScanResults);        
      }
      
      else {
      Debugln(">> End CallBack: Device UUID/Name Unmatch! ", TurnOnScanResults);
      return;
      }

      //Scan can be stopped, if all found
      if (MyClientServerManager->getNumberofClientServer() == sizelimit_def) {
        Serial.println("\t>> Stopping Scan");
        advertisedDevice.getScan()->stop();
        Debugln(">> End CallBack: All Device Added", TurnOnScanResults); 
        return;
      }
      return;
    }
  }  
  
};

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Debugln(">> ClientCallBack: onConnect",true);
  }

  void onDisconnect(BLEClient* pclient) {
    Debugln(">> ClientCallBack: onDisconnect",true);
    //MyClientServerManager->removeDevice(pclient);
  }
};

void setup(){
  //Init BLE device
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);

  BLEDevice::init("");
  delay(500);
  //Start serial communication
  Serial.begin(UART_BAUD);
  // Serial1.begin(UART_BAUD, SERIAL_8N1, BOARD_RX, BOARD_TX);
  Serial1.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println("Starting Arduino BLE Client application...");
  
  //Prints WakeUp Reason
  print_wakeup_reason();
  
  pinMode(StartLogButton,INPUT_PULLUP);
  if (digitalRead(StartLogButton) == LOW){
  //LOW => Stop pairing and start connecting
    Serial.println("Init DONE");
  } else {
  //HIGH => Continue pairing    
    Serial.println("Init START");
  }

  Serial.println(ClientMasterInit ? "ClientInited = true" : "ClientInited = false");

  /* INITIALISING DONE *////////////////////////////////////////////////////////

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  if (!ClientMasterInit && digitalRead(StartLogButton) == HIGH){
    for (int i = 0 ; i < sizelimit_def ; i++){      
      Serial.println("[[CONNECT BEACON VIA PIN]]");
      if (digitalRead(StartLogButton) == HIGH && !AuthenticateServer(ServerAddressArray[i])) break;

      Serial.println("[[SCAN NEW DEVICE]]");
      pBLEScan->start(10, false);
    }
    if (MyClientServerManager->getNumberofClientServer() > 0){
      ClientMasterInit = true;
    }
  }

  else if (ClientMasterInit){
      Serial.println("[[SCAN SAVED DEVICE]]");
      pBLEScan->start(10, false);
  }
  else {
    Serial.print("[[START WITHOUT CHILDREN]]");
  }
  
  connect_All();

  Serial.println("Connected");

  MyClientServerManager->checkAllConnected();
  SDInit = SD_init(MyClientServerManager->TotalConnectedClientServer, SDInit);
  
  /* PAIRING AND SD INITILISE DONE *////////////////////////////////////////////

  Sensor_Start();
  Serial.println("\t>> Sensor Initialised, Start Reading ");
  getValues();
  printReading("BLE Parent", String(temp).c_str(), String(hum).c_str());

  // MyClientServerManager->checkReadings();
  MyClientServerManager->sendReadings(datacount);
  MyClientServerManager->storeAllReadings(datacount);
  datacount++;
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR ); 
    
  Serial.println("Setup ESP32 to sleep for every " + 
  //String((TimeDiff < 0 ? 0 : TimeDiff)) + 
  String(TIME_TO_SLEEP) +
  " Seconds");

  Serial.println("Going to sleep in");
  for (int i = 5 ; i > 0 ; i--){
    Serial.println(String(i)+"...");
    delay(1000);
  }

  Serial.flush();  
  esp_deep_sleep_start();
}

void loop(){
}

int strcicmp(char const *a, char const *b){
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}

bool in_ServerAddressArray(const char * TargetServerCharArray){
  int ind = 0 ; 
  while (ind < sizelimit_def){
    std::string myserver = ServerAddressArray[ind];
    std::string tarserver = TargetServerCharArray;
    std::string empserver =  "00:00:00:00:00:00";
    if (tarserver != empserver && strcmp(ServerAddressArray[ind], TargetServerCharArray) == 0){
      Serial.print(myserver.c_str());
      Serial.print(" Registered : Found ");
      Serial.println(tarserver.c_str());
      return true;
    }
    ind++;
  }
  return false;
}

void connect_All(){
  Serial.println("@Connect_All: Connecting to all Server");  
  for (int i = 0 ; i < sizelimit_def ; i++){
    Serial.print(">> Connecting to Server ");
    Serial.println(i + 1);
    if (MyClientServerManager->MyClientServerList[i]->IsInit && !MyClientServerManager->MyClientServerList[i]->IsConnected){

      if(connect_Server(MyClientServerManager->MyClientServerList[i]->pClient, MyClientServerManager->MyClientServerList[i]->pAdvertisedDevice))
      {
      MyClientServerManager->MyClientServerList[i]->IsConnected = true;      
      MyClientServerManager->MyClientServerList[i]->ReadyCheck.CharReady = true;
      };
    }
  }
  Serial.print(">> End Server Connection: All Connected - ");   
  Serial.printf("Total Device Connected: %d / %d Devices \n", MyClientServerManager->TotalConnectedClientServer, sizelimit_def);
}

bool connect_Server(BLEClient * client_test, BLEAdvertisedDevice * advdev_test){
  Serial.println("\t>> Set and Create Client ");
  client_test = BLEDevice::createClient();
  client_test->setClientCallbacks(new MyClientCallback()); 

  Serial.println("\t>> Connect to server "); 
  client_test->connect(advdev_test); 

  Serial.println("\t>> Get BME Service"); 
  BLERemoteService* pRemoteService = client_test->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("\t\t>>Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    Serial.println(">> End Server Connection: Fail BME Service not found");   
    return (false);
  }
 
  // Obtain a reference to the characteristics in the service of the remote BLE  server.
  Serial.println("\t>> Get Characteristics"); 
  BLERemoteCharacteristic* TempChar = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
  BLERemoteCharacteristic* HumChar = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

  if (TempChar == nullptr || HumChar == nullptr) {
    Serial.println("\t\t>> Failed to find our characteristic UUID");
    Serial.println(">> End Server Connection: Fail Characteristic not found"); 
    return false;
  }
 
  Serial.println("\t>> Assign Notification Callback Functions"); 
  //Assign callback functions for the Characteristics
  TempChar->registerForNotify(temperatureNotifyCallback);
  HumChar->registerForNotify(humidityNotifyCallback);

  
  Serial.println("\t>> Register Characteristics for Notify"); 
  //Activate the Notify property of each Characteristic
  TempChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
  HumChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);

  MyClientServerManager->TotalConnectedClientServer = MyClientServerManager->TotalConnectedClientServer + 1;

  return true;
}

ClientServer::ClientServer(){
  Serial.println("@ClientServer::ClientServer: Initialising ClientServer Object ");    
  Serial.println(">> End Initialisation"); 
}

void ClientServer::setDevice(BLEAdvertisedDevice *p_AdvertisedDevice){
  Serial.println("@ClientServer::setDevice: Setting Device"); 
  Serial.println("\t>> Set Adv Device ");     
  pAdvertisedDevice = p_AdvertisedDevice;
  Serial.println("\t>> Set BLE Address ");     
  pServerAddress = new BLEAddress(p_AdvertisedDevice->getAddress());
  
  Serial.println(">> End Device Set");    
}

// bool ClientServer::connectServer(){
// }

ClientServerManager::ClientServerManager(){
  Serial.println("@ClientServerManager::ClientServerManager: Initialising"); 
  Serial.println("\t>> Assign 1 Clients to MyClientServer ");   
  MyClientServerList[0] = &Client_Test1;
  MyClientServerList[1] = &Client_Test2;
  MyClientServerList[2] = &Client_Test3;
  Serial.println(">> End Initialisation"); 
}

int ClientServerManager::getNumberofClientServer() {
  return TotalClientServer;
}

bool ClientServerManager::addDevice(BLEAdvertisedDevice *pAdvDev){
  Serial.println("@ClientServerManager::addDevice: Adding Device");

  for (int i = 0 ; i < sizelimit ; i++){
    if (!MyClientServerList[i]->IsInit){
      Serial.print("\t>> Found and setDevice for Client_Test ");  
      Serial.print(i);
      Serial.println(" with pAdvDev ");  
      MyClientServerList[i]->setDevice(pAdvDev);
      MyClientServerList[i]->IsInit = true;

      strcpy(ServerAddressArray[i], pAdvDev->getAddress().toString().c_str());

      TotalClientServer = TotalClientServer + 1;

      Serial.println("\t>> Added ");
      Serial.printf(">> %s - %s\n", pAdvDev->getName().c_str(), pAdvDev->getAddress().toString().c_str());
      Serial.print(">> End addDevice: Success - "); 
      Serial.printf("Total Device Added: %d / %d Devices \n", TotalClientServer, sizelimit_def);

      return true;
    }
  }
  return false;
  Serial.println(">> End addDevice: Fail"); 
}

int ClientServerManager::contains(BLEAdvertisedDevice* AdvDev) { 

  Debugln("@ClientServerManager::contains(BLEAdvDevice) ",TurnOnScanResults);
  for (int i = 0; i < sizelimit_def; i++) {
    if (MyClientServerList[i]->IsInit){
      BLEAddress OtherServer = *MyClientServerList[i]->pServerAddress;

      if (AdvDev->getAddress().equals(OtherServer)) {
        Debugln("\t>> BLEAdvDevice Found: true ",TurnOnScanResults);
        return i;
      }
    }    
  }
  
  Debugln(">> End contains(BLEAdvDevice): BLEAdvDevice not Found: false ",TurnOnScanResults);
  return -1;
}

bool ClientServerManager::checkAllConnected() {
  Serial.println("@ClientServerManager::checkAllConnected: Checking if All Server Connected");
  Serial.print("\t>> ServerCheck: ");
  if (sizelimit_def == TotalClientServer) {
    Serial.printf(">> End checkAllConnected: All Server Connected: %d / %d Devices \n", TotalConnectedClientServer, sizelimit_def);
    return true;
  } else {
    Serial.printf(">> End checkAllConnected: Not all Server Connected: %d / %d Devices \n", TotalConnectedClientServer, sizelimit_def);
    return false;
  }
  Serial.println("");
}

bool ClientServerManager::checkAllReady() {
  Serial.println("@ClientServerManager::checkAllReady: Checking if All Characteristics Ready");
  int count = 0;
  if (MyClientServerManager->getNumberofClientServer() == 0) {
    Serial.println("No Server to check!");
    return false;
  }
  for (int i = 0; i < sizelimit; i++) {
    Serial.print("\t>> Next Server: ");
    /* Testing Purpose
      Serial.print(MyClientServerManager->MyClientServerList[i]->IsConnected ? "Conn - " : "!Conn - ");
      Serial.print(MyClientServerManager->MyClientServerList[i]->IsInit ? "Init - " : "!Init - ");
      Serial.print(MyClientServerManager->MyClientServerList[i]->ReadyCheck.CharReady ? "Char - " : "!Char - ");
      Serial.print(MyClientServerManager->MyClientServerList[i]->ReadyCheck.TempReady ? "Temp - " : "!Temp - ");
      Serial.print(MyClientServerManager->MyClientServerList[i]->ReadyCheck.HumReady ? "Hum - " : "!Hum - ");
    */

    if (MyClientServerManager->MyClientServerList[i]->IsConnected
      && MyClientServerManager->MyClientServerList[i]->IsInit
      && MyClientServerManager->MyClientServerList[i]->ReadyCheck.CharReady
      && MyClientServerManager->MyClientServerList[i]->ReadyCheck.TempReady
      && MyClientServerManager->MyClientServerList[i]->ReadyCheck.HumReady
      ) {
      std::string SavedServer = MyClientServerManager->MyClientServerList[i]->pServerAddress->toString();

      Serial.printf("Checking Target ClientServer (%s) : ", SavedServer.c_str());
      Serial.println("Success: Ready!");
      count += 1;
    } else {
      Serial.println("Failed: Not Added/ Not Ready");
    }
  }
  return count == MyClientServerManager->TotalConnectedClientServer;
}

void ClientServerManager::setTempChar(std::string Server, std::string Value) {  
  // Method/function defined inside the class
 Debugf("@ClientServerManager::setTempChar: Setting Temp Char for Server(%s): \n", Server.c_str(), TurnOnNotiResults);
  if (MyClientServerManager->getNumberofClientServer() == 0) {
    Debugln("No Server to set!", TurnOnNotiResults);
    return;
  }
  for (int i = 0; i < sizelimit; i++) {
    Debug("\t>> Next Server: ", TurnOnNotiResults);
    std::string SavedServer = MyClientServerManager->MyClientServerList[i]->pServerAddress->toString();

    Debugf("Checking (%s) : ", SavedServer.c_str(), TurnOnNotiResults);

    if (strcmp(SavedServer.c_str(), Server.c_str()) == 0) {
      MyClientServerManager->MyClientServerList[i]->NewData.NewTemp = Value;
      MyClientServerManager->MyClientServerList[i]->ReadyCheck.TempReady = true;
      Debugln("Success: TempChar Set for Print", true);
      return;
    } else {
      Debug("Failed: Server Address Unmatch", TurnOnNotiResults);
    }
  }
  return;
}

void ClientServerManager::setHumChar(std::string Server, std::string Value) {  
  // Method/function defined inside the class
  Debugf("@ClientServerManager::setHumChar: Setting Hum Char for Server(%s): \n", Server.c_str(), TurnOnNotiResults);

  if (MyClientServerManager->getNumberofClientServer() == 0) {
    Debugln("No Server to set!", TurnOnNotiResults);
    return;
  }

  for (int i = 0; i < sizelimit; i++) {
    Debug("\t>> Next Server: ", TurnOnNotiResults);
    std::string SavedServer =  MyClientServerManager->MyClientServerList[i]->pServerAddress->toString();
    Debugf("Checking (%s) : ", SavedServer.c_str(), TurnOnNotiResults);
    if (strcmp(SavedServer.c_str(), Server.c_str()) == 0) {
      MyClientServerManager->MyClientServerList[i]->NewData.NewHum = Value;
      MyClientServerManager->MyClientServerList[i]->ReadyCheck.HumReady = true;
      Debugln("Success: HumChar Set for Print", true);
      return;
    } else {
      Debugln("Failed: Server Address Unmatch", TurnOnNotiResults);
    }
  }
  return;
}

// bool ClientServerManager::checkReadings(){
//   Serial.println("@ClientServerManager::checkReadings: Check and Average");  
    
//   Serial.println("End checkReadings: Done");  
// }

void ClientServerManager::sendReadings(int &currdatacount ) {
  Serial.println("@ClientServerManager::sendReadings: Check and Post All");  

  Serial.println("\t>> Checking that all ClientServer is Ready");
  int checkStart = millis();
  while (!MyClientServerManager->checkAllReady() && (millis()-checkStart)<1550) {
    delay(500);
    Serial.println("\t\t>> Not All Ready");
  }

  AverageReadings();

  //Start Modem and GPRS, Get GPS and Datetime, Post to Cloud
  Serial.println("(int)(currdatacount-dataindex_last + 1)");
  Serial.println((int)(currdatacount-dataindex_last + 1));
  Serial.println("(int)(currdatacount-dataindex_last + 1)%READING_BEFORE_SEND)");
  Serial.println((int)(currdatacount-dataindex_last + 1)%READING_BEFORE_SEND);

  if (0 == (int)(currdatacount-dataindex_last + 1)%READING_BEFORE_SEND || (currdatacount == 0 && dataindex_last == 0)){
    Serial.println("\t>> Use DTR Pin Wakeup");
    pinMode(MODEM_DTR, OUTPUT);
    //Set DTR Pin low , wakeup modem .
    digitalWrite(MODEM_DTR, LOW);
    delay(300);
    Serial.println("\t>> Starting modem ... ");
    TurnOnSIMModule();
    StartModem();
    if (StartGPRS()){
      Serial.println("\t>> GRPS Connection Success");
      GetTime_TinyGSM(); 
      Serial.println("\t>> TinyGSM Client POST ALL to Sussy");
      if (!SendAllPostHttpRequest_TinyGSM(currdatacount)){
        Serial.println("\t>> Post Requests Failed");
      }
      else{
        Serial.println("\t>> Post Requests Success");
      };
      //dataindex_last++ occurs in PostSendAll
    }
    else {
      Serial.println("\t>> GRPS Connection Failed");
      GetNextTime();
    };
    Serial.println("\t>> Retrieving GPS Information");
    GetGPS();
    Serial.println("\t>> Stopping SIM Module and network functions");
    StopModem();
    Serial.println("End sendReadings: Readings Remaining : " + String(currdatacount - dataindex_last + 1));  
    return;
  }else{
    Serial.println("\t>> GetTime due to skipping");
    GetNextTime();
  }
  Serial.println("End sendReadings: Readings Skipped : " + String(currdatacount - dataindex_last + 1));  
}

void ClientServerManager::storeAllReadings(int &currdatacount ) {
  Serial.println("@ClientServerManager::storeAllReadings: Store All");  

  Serial.println("\t>> Storing Self and Averaged Readings in CSV");
  storeReading("/avrg.csv", currdatacount, CurrTemp_Cloud, CurrHum_Cloud, currDateTimePOST, CurrLat , CurrLong);
  storeReading("/data.csv", currdatacount, temp, hum, currDateTimeCSV, CurrLat , CurrLong);

  Serial.println("\t>> Storing Readings from all ClientServer");

  char * DataCSVPath[] = {"/data1.csv","/data2.csv","/data3.csv"};
  
  for (int i = 0; i < MyClientServerManager->TotalConnectedClientServer; i++) {

    Serial.print("\t>> Next Server: ");
    ClientServer * CurrServer = MyClientServerManager->MyClientServerList[i];
    if (CurrServer->IsConnected && CurrServer->IsInit 
    && CurrServer->ReadyCheck.CharReady && CurrServer->ReadyCheck.HumReady 
    && CurrServer->ReadyCheck.TempReady) {

      std::string SavedServer = CurrServer->pServerAddress->toString().c_str();
      printReading(SavedServer.c_str(), String(temp).c_str(), String(hum).c_str());
      
      //Store in SD
      storeReading(DataCSVPath[i], currdatacount, CurrServer->NewData.NewTemp.c_str(), CurrServer->NewData.NewTemp.c_str(),currDateTimeCSV, CurrLat , CurrLong);
      
      CurrServer->ReadyCheck.HumReady = false;
      CurrServer->ReadyCheck.TempReady = false;
    }

    else Serial.println("BLE Server Unavailable/ Unready");
  }
  Serial.println("End checkReadings: All Readings Saved");  
}

static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {

  Debug("[[Notification (TEMP)]] @ ",true);
  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();

  Debugf(" from Server (%s) Notified \n", ServerAddressStr.c_str(),true);
  char* temperatureChar = (char*)pData;
  temperatureChar[6] = '\0';
  std::string TempString = temperatureChar;
  Debugf("Temperature: %s C \n", TempString.c_str(),TurnOnNotiResults);
  Debug("-> Call setTempChar: ",TurnOnNotiResults);
  MyClientServerManager->setTempChar(ServerAddressStr, TempString);
  Debugln("",true);
}
//When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,uint8_t* pData, size_t length, bool isNotify) {

  Debug("[[Notification (HUM)]] @ ",true);
  std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();

  Debugf(" from Server (%s) Notified \n", ServerAddressStr.c_str(),true);
  char* humidityChar = (char*)pData;
  humidityChar[6] = '\0';
  std::string HumString = humidityChar;
  Debugf("Humidity: %s %% \n", HumString.c_str(),TurnOnNotiResults);
  Debug("-> Call setHumChar: ",TurnOnNotiResults);
  MyClientServerManager->setHumChar(ServerAddressStr, HumString);
  Debugln("",true);
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
    default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void storeReading(const char* filename, int data_count, float curr_Temp, float curr_Hum, String curr_DateTime, float curr_Lat, float curr_Lon){
  //Store Self reading in SD
  String dataString = String(data_count) + "," 
  + String(curr_Temp) + "," + String(curr_Hum) + "," 
  + curr_DateTime + "," 
  + String(curr_Lat) + "," + String(curr_Lon);
  Serial.print("\t>> Appending : " + dataString + " >> ");

  if (SDInit && writeDataLine(SD, filename, dataString.c_str())) {
    Serial.println("\t>> New Data added");
    readFile(SD, filename);
  } else {
    Serial.println("\t>> New Data not added");
  }
}

void storeReading(const char* filename, int data_count, const char* curr_Temp, const char* curr_Hum, String curr_DateTime, float curr_Lat, float curr_Lon){
  //Store Self reading in SD
  String dataString = String(data_count) + "," 
  + String(curr_Temp) + "," + String(curr_Hum) + "," 
  + curr_DateTime + "," 
  + String(curr_Lat) + "," + String(curr_Lon);
  Serial.print("\t>> Appending : " + dataString + " >> ");

  if (SDInit && writeDataLine(SD, filename, dataString.c_str())) {
    Serial.println("\t>> New Data added");
    // readFile(SD, filename);
  } else {
    Serial.println("\t>> New Data not added");
  }
}

void printReading(const char* savedServer, const char* curr_Temp, const char* curr_Hum){
      Serial.printf("\t>> BLE Device : %s \n", savedServer);
      Serial.printf("\t>> Readings: Temperature = %s C - ", curr_Temp);
      Serial.printf("Humidity = %s %% \n",curr_Hum);
}

void AverageReadings(){
  Serial.println(">> AverageReadings : Collate and Average Self and Beacon Readings");
  int TotalCurrReadings = 1 + MyClientServerManager->TotalConnectedClientServer;
  float CurrTempReadingsSum = temp;
  float CurrHumReadingsSum = hum;
  
  for (int i = 0 ; i < MyClientServerManager->TotalConnectedClientServer ; i++){
    ClientServer * CurrServer = MyClientServerManager->MyClientServerList[i];
    CurrTempReadingsSum += std::stof(CurrServer->NewData.NewTemp);
    CurrHumReadingsSum += std::stof(CurrServer->NewData.NewHum);
  }
  //Get Average
  CurrTemp_Cloud = CurrTempReadingsSum/TotalCurrReadings;
  CurrHum_Cloud = CurrHumReadingsSum/TotalCurrReadings;
  Serial.println("\t>> Total Readings to Average: " + String(TotalCurrReadings));
  Serial.println("\t>> Avg Temp : " + String(CurrTemp_Cloud) + " - " + "Avg Hum : " + String(CurrHum_Cloud));
  Serial.println(">> End AverageReadings : Readings Averaged");
}

void AddHours( struct tm* date) {
    // const time_t DEEP_SLEEP_TIME = HRS_TO_SLEEP * 60 * 60 ;
    const time_t DEEP_SLEEP_TIME = TIME_TO_SLEEP ;
    const time_t p_date_seconds = mktime( date )  ;
    Serial.println("\t>> Previous DateTime: " + String(ctime( &p_date_seconds ))); 

    // Seconds since start of epoch
    const time_t date_seconds = mktime( date ) + (DEEP_SLEEP_TIME) ;
    Serial.println("\t>> Current DateTime: " + String(ctime(&date_seconds)));
    
    // Update caller's date
    // Use localtime because mktime converts to UTC so may change date
    *date = *localtime( &date_seconds ) ; ;
}

void GetNextTime(){ 
  Serial.println(">> GetNextTime : Add Sleep time manually to last saved time");
  struct tm date = { 0, 0, 12 } ;   // nominal time midday (arbitrary).

  // Set up the date structure
  date.tm_year = LastYr - 1900 ;
  date.tm_mon = LastMth - 1 ;     // note: zero indexed
  date.tm_mday = LastDay ;        // note: not zero indexed
  date.tm_hour = LastHr ;
  date.tm_min = LastMin ;
  date.tm_sec = LastSec ;
  AddHours(&date);

  LastYr = date.tm_year + 1900;
  LastMth = date.tm_mon + 1;
  LastDay = date.tm_mday;
  LastHr = date.tm_hour;
  LastMin = date.tm_min;
  LastSec = date.tm_sec;

  currDateTimePOST = String(LastYr) + "-" 
  + (LastMth < 10 ? String("0") + String(LastMth) : String(LastMth)) + "-" 
  + (LastDay < 10 ? String("0") + String(LastDay) : String(LastDay)) +  "T" 
  + (LastHr < 10 ? String("0") + String(LastHr) : String(LastHr)) + ":" 
  + (LastMin < 10 ? String("0") + String(LastMin) : String(LastMin)) + ":" 
  + (LastSec < 10 ? String("0") + String(LastSec) : String(LastSec)) 
  + toTimezoneString(LastTZ);

  currDateTimeCSV = String(LastYr) + "-" 
  + (LastMth < 10 ? String("0") + String(LastMth) : String(LastMth)) + "-" 
  + (LastDay < 10 ? String("0") + String(LastDay) : String(LastDay)) +  "T" 
  + (LastHr < 10 ? String("0") + String(LastHr) : String(LastHr)) + ":" 
  + (LastMin < 10 ? String("0") + String(LastMin) : String(LastMin)) + ":" 
  + (LastSec < 10 ? String("0") + String(LastSec) : String(LastSec))  
  + toTimezoneStringCSV(LastTZ);

  Serial.println("End GetNextTime : DateTime adjusted");
}

//LILYGO/TinyGSM Commands
  void TurnOnSIMModule(){
    Serial.println("\t>> TinyGSM: Turning on SIM Module");
    /*
      MODEM_PWRKEY IO:4 The power-on signal of the modulator must be given to it,
      otherwise the modulator will not reply when the command is sent
    */
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(300);  //Need delay
    digitalWrite(MODEM_PWRKEY, LOW);

    /*
      MODEM_FLIGHT IO:25 Modulator flight mode control,
      need to enable modulator, this pin must be set to high
    */
    pinMode(MODEM_FLIGHT, OUTPUT);
    digitalWrite(MODEM_FLIGHT, HIGH);
  }

  void StartModem() {
    Serial.println("@TingyGSM::StartModem: Initializing modem");
    if (!modem.init()) {
      Serial.println("\t>> Failed to restart modem, delaying 2s and retrying");
      delay(100);
      light_sleep(2);
    }
    Serial.println("End StartModem: Initialize OK");
  }

  void StopModem() {
    Serial.println("@TingyGSM::StopModem: Stopping GPRS and modem");
    modem.gprsDisconnect();
    delay(200);
    if (!modem.isGprsConnected()) {
      Serial.println("\t>> GPRS disconnected");
    } else {
      Serial.println("\t>> GPRS disconnect: Failed.");
    }
    modem.sleepEnable();
    delay(100);
    modem.poweroff();
    pinMode(4, INPUT_PULLUP);
    Serial.println("\t>> Waiting for modem to powerdown9");
    light_sleep(5);
    Serial.println("End StopModem: Stopped GPRS and modem");
  }

  bool StartGPRS() {
    Serial.println("@TingyGSM::StartGPRS: Starting GPRS Service");
    // Set Modem to Auto, Set GNSS
    /*  Preferred mode selection : AT+CNMP
      2 – Automatic

      do {
        ret = modem.setNetworkMode(2);
        delay(500);
      } while (ret != "OK");
    */

    String ret;
    ret = modem.setNetworkMode(2);
    Serial.println("\t\>> setNetworkMode: "+ ret);


    //https://github.com/vshymanskyy/TinyGSM/pull/405
    uint8_t mode = modem.getGNSSMode();
    Serial.println("\t>> GNSS Mode: " + String(mode));

    /**
        CGNSSMODE: <gnss_mode>,<dpo_mode>
        This command is used to configure GPS, GLONASS, BEIDOU and QZSS support mode.
        gnss_mode:
            0 : GLONASS
            1 : BEIDOU
            2 : GALILEO
            3 : QZSS
        dpo_mode :
            0 disable
            1 enable
    */
    modem.setGNSSMode(0, 1);
    light_sleep(2);

    String name = modem.getModemName();
    Serial.println("\t>> Modem Name: " + name);

    String modemInfo = modem.getModemInfo();
    Serial.println("\t>> Modem Info: " + modemInfo);

    // Unlock your SIM card with a PIN if needed
    if (GSM_PIN && modem.getSimStatus() != 3) {
      modem.simUnlock(GSM_PIN);
    }

    Serial.println("\t>> Waiting for network...");
    delay(1000);
    if (!modem.waitForNetwork(10000L)) {
      Serial.println("\t\t>> Wait Network Light Sleep 3 Sec");
      delay(200);
      light_sleep(3);
      // return;
    }

    if (modem.isNetworkConnected()) {
      Serial.println("\t>> Network connected");
    } else {
      Serial.println("\t>> Network not connected");
      return false;
    }
      
    // Connect to APN
    Serial.print("\t>> Connecting to APN: ");
    Serial.println(APN);
    DBG("Connecting to", APN);
    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
      Serial.println("\t>> APN Light Sleep 2 Sec");
      delay(100);
      light_sleep(2);
    } 

    //res will represent the modem connection's success/ failure
    bool res = modem.isGprsConnected();
    Serial.println("\t>> GPRS status: " + String(res ? "connected" : "not connected"));
    if (!res) return false;

    String ccid = modem.getSimCCID();
    Serial.println("\t>> CCID: " + ccid);

    String imei = modem.getIMEI();
    Serial.println("\t>> IMEI: " +  imei);

    String imsi = modem.getIMSI();
    Serial.println("\t>> IMSI: " + imsi);

    String cop = modem.getOperator();
    Serial.println("\t>> Operator: " + cop);

    IPAddress local = modem.localIP();
    Serial.println("\t>> Local IP: " + local.toString());

    int csq = modem.getSignalQuality();
    Serial.println("\t>> Signal quality : " + String(csq));
    Serial.println("End StartGPRS: GPRS Started");
    return true;
  }

  void SendPostHttpRequest_TinyGSM() {
    Serial.println("@TingyGSM::SendPostHttpRequest_TinyGSM: Starting Post Request");

    const char server[] = "sussylogger-2.fly.dev";  //
    const int port = 80;

    TinyGsmClient client(modem);
    HttpClient http = HttpClient(client, server, port);

    int shipmentID = CurrShippingID;

    String contentType = "application/x-www-form-urlencoded";
    String postData =   "shipmentID=" + String(shipmentID) 
                      // + "&childid=" + childName 
                      + "&temperature=" + String(CurrTemp_Cloud) 
                      + "&humidity=" + String(CurrHum_Cloud) 
                      + "&latitude=" + String(CurrLat)
                      + "&nS=N" 
                      + "&longitude=" + String(CurrLong) 
                      + "&eW=E"
                      + "&DateTime=" + currDateTimePOST;

    Serial.print("\t>> Performing HTTP POST request to ... ");
    Serial.println(server);
    Serial.print("\t>> Posting: ");
    Serial.println(postData);

    http.beginRequest();
    int err = http.post("/api/master/updateShipment");
    Serial.println("\t>> Error Code: " + String(err));
    http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
    http.sendHeader("Content-Length", postData.length());
    http.beginBody();
    http.print(postData);
    http.endRequest();

    int status = http.responseStatusCode();
    Serial.print("\t>> Response status code: "+String(status));
    if (!status) {
      delay(2000);
    }
    if (status == 200){      
      dataindex_last++;
    } 

    // Serial.println("\t>> Response Headers:");
    // if (http.headerAvailable()) {
    //   String headerName = http.readHeaderName();
    //   String headerValue = http.readHeaderValue();
    //   Serial.println("    " + headerName + " : " + headerValue);
    // }

    // int length = http.contentLength();
    // if (length >= 0) {
    //   Serial.println("\t>> Content length is: " + String(length));
    // }
    // if (http.isResponseChunked()) {
    //   Serial.println("\t>> The response is chunked");
    // }

    // String body = http.responseBody();
    // Serial.println("\t>> Response:" + body);

    // Serial.print("\t>> Body length is: ");
    // Serial.println(body.length());

    http.stop();
    Serial.println("\t>> Server disconnected");
    Serial.println("End SendPostHttpRequest_TinyGSM: Post request success");
  }

  bool SendAllPostHttpRequest_TinyGSM(int &currdatacount) {
    Serial.println("@TingyGSM::SendAllPostHttpRequest_TinyGSM: Starting Post Request for All Devices");

    const char server[] = "sussylogger-2.fly.dev";  
    const char URLPath[] = "/api/master/updateShipment";
    const int port = 80;
    int shipmentID = CurrShippingID;
    int currindex = -1;
    float Temp_Cloud;
    float Hum_Cloud;
    char DateTime_Cloud[27+1]  , temp[100];
    float Lat_Cloud;
    float Long_Cloud;
    int PostRetries = 0;

    Serial.println("\t>> Creating HttpClient");
    TinyGsmClient client(modem);
    HttpClient http = HttpClient(client, server, port);

    Serial.println("\t>> Reading from avrg.csv");
    File file = SD.open("/avrg.csv");
    readLine(file, temp ,100);
    Serial.println(temp);

    while(currindex < dataindex_last - 1){      
      readLine(file, temp ,100);
      Serial.print("\t\t>> Past Reading: ");
      Serial.println(temp);
      currindex = std::atoi(temp);
    }

    Serial.println("\t>> Posting Unsent Readings");
    while(dataindex_last < currdatacount ){
      Serial.println("\t>> Curr Datacount: "+ String(currdatacount) + 
            "\tLast Unsent: " + String(dataindex_last));

      Serial.println("\t>> Reading Idx: " + String(currindex+1));
      if (readVals(file,&currindex,&Temp_Cloud,&Hum_Cloud,DateTime_Cloud,27,&Lat_Cloud,&Long_Cloud)){
        Serial.println("\t\t>> Read OK");
      }else{
        Serial.println("\t\t>> Read Failed");
        break;
      };
      DateTime_Cloud[27] = '\0';
      PostRetries = 0;
      while (PostRetries < 3){
        Serial.println("\t>> Posting Idx: " + String(currindex) + "\tTries: " + String(PostRetries+1));
        Serial.println("\t>> Posting to: " + String(server) + "\tPath: " + String(URLPath));
        // SendHttpRequest(&http, URLPath, shipmentID, Temp_Cloud, Hum_Cloud, DateTime_Cloud, Lat_Cloud, Long_Cloud);

        String contentType = "application/x-www-form-urlencoded";
        String postData =   "shipmentID=" + String(shipmentID) 
                          // + "&childid=" + childName 
                          + "&temperature=" + String(Temp_Cloud) 
                          + "&humidity=" + String(Hum_Cloud) 
                          + "&latitude=" + String(Lat_Cloud)
                          + "&nS=N"
                          + "&longitude=" + String(Long_Cloud) 
                          + "&eW=E"
                          + "&DateTime=" + String(DateTime_Cloud);

        Serial.println("\t\t>> Post Body: " + postData);

        http.beginRequest();
        int err = http.post("/api/master/updateShipment");
        Serial.println("\t\t>> Error Code: " + String(err));
        http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
        http.sendHeader("Content-Length", postData.length());
        http.beginBody();
        http.print(postData);
        http.endRequest();
        Serial.println("\t\t>> End Post Request");

        int status = http.responseStatusCode();
        Serial.println("\t\t>> Response status code: " + String(status));
        PostRetries++;
        if (status == 200){
          dataindex_last++;
          break;
        }
        if (PostRetries == 3){
          Serial.println("End SendAllPostHttpRequest_TinyGSM: Post request Failed");
          return false;
        }
      }
    }

    file.close();

    if (currdatacount==dataindex_last) {
      Serial.println("\t>> Curr Datacount: "+ String(currdatacount) + 
            "\tLast Unsent: " + String(dataindex_last));
      PostRetries = 0;
      while(PostRetries<3){
        Serial.println("\t>> Posting Idx: " + String(currdatacount) + "\tTries: " + String(PostRetries+1));
        Serial.println("\t>> Posting to: " + String(server) + "\tPath: " + String(URLPath));
        // SendHttpRequest(&http, URLPath, shipmentID, CurrTemp_Cloud, CurrHum_Cloud, currDateTimePOST, CurrLat, CurrLong);

        String contentType = "application/x-www-form-urlencoded";
        String postData =   "shipmentID=" + String(shipmentID) 
                          // + "&childid=" + childName 
                          + "&temperature=" + String(CurrTemp_Cloud) 
                          + "&humidity=" + String(CurrHum_Cloud) 
                          + "&latitude=" + String(CurrLat)
                          + "&nS=N" 
                          + "&longitude=" + String(CurrLong) 
                          + "&eW=E"
                          + "&DateTime=" + currDateTimePOST;

        Serial.println("\t\t>> Post Body: " + postData);

        http.beginRequest();
        int err = http.post("/api/master/updateShipment");
        Serial.println("\t\t>> Error Code: " + String(err));
        http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
        http.sendHeader("Content-Length", postData.length());
        http.beginBody();
        http.print(postData);
        http.endRequest();
        Serial.println("\t\t>> End Post Request");

        int status = http.responseStatusCode();
        Serial.println("\t\t>> Response status code: " + String(status));
        PostRetries++;
        if (status == 200){
          dataindex_last++;
          return true;
        }
        if (PostRetries == 3){
          Serial.println("End SendAllPostHttpRequest_TinyGSM: Post request Failed");
          return false;
        }
      }
    }

    http.stop();
    Serial.println("\t>> Server disconnected");
    Serial.println("End SendAllPostHttpRequest_TinyGSM: Post request success");
  }

  void SendGetHttpRequest_TinyGSM() {

    const char server[] = "sussylogger-2.fly.dev";  //
    const char resource[] = "/";
    const int port = 80;

    TinyGsmClient client(modem);
    HttpClient http(client, server, port);
    Serial.print(F("Performing HTTP GET request to ... "));
    Serial.print(F(server));
    // http.connectionKeepAlive();  // Currently, this is needed for HTTPS
    int err = http.get(resource);
    if (err != 0) {
      Serial.println(F(" failed to connect"));
      delay(10000);
      // return;
    }
    Serial.println(" ");

    int status = http.responseStatusCode();
    Serial.print(F("Response status code: "));
    Serial.println(status);
    if (!status) {
      delay(10000);
      // return;
    }

    // Serial.println(F("Response Headers:"));
    // if (http.headerAvailable()) {
    //   String headerName = http.readHeaderName();
    //   String headerValue = http.readHeaderValue();
    //   Serial.println("    " + headerName + " : " + headerValue);
    // }

    // int length = http.contentLength();
    // if (length >= 0) {
    //   Serial.print(F("Content length is: "));
    //   Serial.println(length);
    // }
    // if (http.isResponseChunked()) {
    //   Serial.println(F("The response is chunked"));
    // }

    String body = http.responseBody();
    Serial.println(F("Response:"));
    Serial.println(body);

    Serial.print(F("Body length is: "));
    Serial.println(body.length());

    // Shutdown

    http.stop();
    Serial.println(F("Server disconnected"));
  }

  void GetTime_TinyGSM() {
    int year3 = 0;
    int month3 = 0;
    int day3 = 0;
    int hour3 = 0;
    int min3 = 0;
    int sec3 = 0;
    float timezone = 0;
    for (int8_t i = 5; i; i--) {
      DBG("Requesting current network time");
      if (modem.getNetworkTime(&year3, &month3, &day3, &hour3, &min3, &sec3,
                              &timezone)) {
        DBG("Year:", year3, "\tMonth:", month3, "\tDay:", day3);
        DBG("Hour:", hour3, "\tMinute:", min3, "\tSecond:", sec3);
        DBG("Timezone:", timezone);
        break;
      } else {
        DBG("Couldn't get network time, retrying in 3s.");
        light_sleep(3);
      }
    }
    LastYr = year3;
    LastMth = month3;
    LastDay = day3;
    LastHr = hour3;
    LastMin = min3;
    LastSec = sec3;
    LastTZ = timezone;
    DBG("Retrieving time again as a string");
    String datefull = modem.getGSMDateTime(DATE_FULL);
    String datetime = modem.getGSMDateTime(DATE_TIME);
    String datedate = modem.getGSMDateTime(DATE_DATE);

    //23/03/03,00:39:54+32
    //2023-02-28T19:20:01
    currDateTimePOST = String(year3) + "-" + datefull.substring(3,5) + "-" + datefull.substring(6,8) + "T" + datetime.substring(0,2) + ":" + datetime.substring(3,5) + ":" + datetime.substring(6,8) + toTimezoneString(timezone);

    currDateTimeCSV = String(year3) + "-" + datefull.substring(3,5) + "-" + datefull.substring(6,8) + "T" + datetime.substring(0,2) + ":" + datetime.substring(3,5) + ":" + datetime.substring(6,8) + toTimezoneStringCSV(timezone);

    currDateTimePOST = currDateTimePOST.c_str();
    Serial.println("Current Network Date_Full: " + datefull) ;
    Serial.println("Current Network Date_Time: " + datetime);
    Serial.println("Current Network Date_Date: " + datedate);
    Serial.println("Current Network My Time: " + currDateTimeCSV);
    Serial.println("Current TimeZone: " + String(timezone));
  }

  void GetGPS() {
    DBG("Enabling GPS/GNSS/GLONASS");
    Serial.println("Enabling GPS/GNSS/GLONASS");
    modem.enableGPS();
    delay(500);
    float lat2 = 0;
    float lon2 = 0;
    float speed2 = 0;
    float alt2 = 0;
    int vsat2 = 0;
    int usat2 = 0;
    float accuracy2 = 0;
    int year2 = 0;
    int month2 = 0;
    int day2 = 0;
    int hour2 = 0;
    int min2 = 0;
    int sec2 = 0;
    Serial.println("Requesting current GPS/GNSS/GLONASS location");
    DBG("Requesting current GPS/GNSS/GLONASS location");

    if (modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                    &year2, &month2, &day2, &hour2, &min2, &sec2)) {
      DBG("Latitude:", String(lat2, 8), "\tLongitude:", String(lon2, 8));
      DBG("Speed:", speed2, "\tAltitude:", alt2);
      DBG("Visible Satellites:", vsat2, "\tUsed Satellites:", usat2);
      DBG("Accuracy:", accuracy2);
      DBG("Year:", year2, "\tMonth:", month2, "\tDay:", day2);
      DBG("Hour:", hour2, "\tMinute:", min2, "\tSecond:", sec2);
    } 

    CurrLat = lat2;
    CurrLong = lon2;
    DBG("Retrieving GPS/GNSS/GLONASS location again as a string");
    static String gps_raw = modem.getGPSraw();
    
    DBG("GPS/GNSS Based Location String:", gps_raw);
    Serial.print("GPS/GNSS Based Location String: ");
    Serial.println(gps_raw);
    DBG("Disabling GPS");
    Serial.println("Disabling GPS");
    modem.disableGPS();
  }
  
  String sendData(String command, const int timeout, boolean debug){
    String response = "";
    SerialAT.println(command);
    
    long int time = millis();
    while ( (time + timeout) > millis())
    {
      while (SerialAT.available())
      {
        char c = SerialAT.read();
        response += c;
      }
    }
    if (debug)
    {
      SerialAT.print(response);
    }
    return response;
  }
  
  String toTimezoneString(float timezone){
    int tzone = int( timezone * 100);
    tzone = tzone<0?-tzone:tzone;
    div_t result1 = div(tzone, 100);
    int quot = result1.quot;
    int rem = result1.rem * 60 / 100;
    String hr = quot < 1000 ? "0" + String(quot):String(quot);
    String min = rem < 10 ? "0" + String(rem):String(rem);
    if (timezone>= 0.0){
      return "%2B"+ hr + ":" + min;

    }else{
      return "%2D"+ hr + ":" + min;

    }
  }

   String toTimezoneStringCSV(float timezone){
    int tzone = int( timezone * 100);
    tzone = tzone<0?-tzone:tzone;
    div_t result1 = div(tzone, 100);
    int quot = result1.quot;
    int rem = result1.rem * 60 / 100;
    String hr = quot < 1000 ? "0" + String(quot):String(quot);
    String min = rem < 10 ? "0" + String(rem):String(rem);
    if (timezone>= 0.0){
      return "+"+ hr + ":" + min;

    }else{
      return "-"+ hr + ":" + min;

    }
  }

  void light_sleep(uint32_t sec) {
    esp_sleep_enable_timer_wakeup(sec * 1000000ULL);
    esp_light_sleep_start();
  }
////////////////////////////////////////////////////////////////////////////////


/*MISC*/

  void OffAll(){
      for (int ind = 0; ind <= sizelimit_def ;ind++){
      digitalWrite(LED_arr[ind],LOW); 
    };

  }

  void OnAll(){
      for (int ind = 0; ind <= sizelimit_def ;ind++){
      digitalWrite(LED_arr[ind],HIGH); 
    };
    // digitalWrite(LED_PIN_MAIN, HIGH);
    // digitalWrite(LED_PIN_GREEN_1, HIGH);
    // digitalWrite(LED_PIN_GREEN_2, HIGH);
    // digitalWrite(LED_PIN_GREEN_3, HIGH);

  }

  void flashOnce(int pin_num , int milliSec){  
    digitalWrite(pin_num, HIGH);
    delay(milliSec);
    digitalWrite(pin_num, LOW);
  }

  void Blink(int pin_num , int milliSec){  
    digitalWrite(pin_num, HIGH);
    delay(milliSec);
    digitalWrite(pin_num, LOW);
    delay(milliSec);
  }

  void Bounce(){
    int ind = 0 ;
    int del = 300;
    for (; ind < sizelimit_def + 1 ;ind++){
      Serial.println(String(ind));
      flashOnce(LED_arr[ind],del); 
    };
    
    for (ind = sizelimit_def - 1 ; ind >= 0 ;ind--){
      Serial.println(String(ind));
      flashOnce(LED_arr[ind],del); 
    };
    delay(del);
  }

  void Debugf(const char* main, const char* format, bool bswitch) {
    if (bswitch) {
      Serial.printf(main, format);
    }
  }
  
  void Debug(const char* main, bool bswitch) {
    if (bswitch) {
      Serial.print(main);
    }
  }
  
  void Debugln(const char* main, bool bswitch) {
    if (bswitch) {
      Serial.println(main);
    }
  }

////////////////////////////////////////////////////////////////////////////////

