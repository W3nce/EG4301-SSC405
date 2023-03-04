
#include "utilities.h"
#include "Lilygo_multi.h"
#include "UARTSerial_utils.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

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

//Activate notify
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

bool connected = false;

//Scan Object Pointer
static BLEScan* pBLEScan;

// //Address of the advertised Server found during scanning...
// static BLEAddress *pServerAddress;
// //Reference to the advertised Server found during scanning...
// static BLEAdvertisedDevice *pAdvertisedDevice;

//Characteristicd that we want to read
// static BLERemoteCharacteristic* temperatureCharacteristic;
// static BLERemoteCharacteristic* humidityCharacteristic;

static ClientServerManager * MyClientServerManager = new ClientServerManager();
RTC_DATA_ATTR static bool ClientMasterInit = false;
RTC_DATA_ATTR static int TotalConnected = 0;

RTC_DATA_ATTR static char add1[ServerAddressLength + 1] = "00:00:00:00:00:00";
RTC_DATA_ATTR static char add2[ServerAddressLength + 1] = "00:00:00:00:00:00";
RTC_DATA_ATTR static char add3[ServerAddressLength + 1] = "00:00:00:00:00:00";
RTC_DATA_ATTR static char * ServerAddressArray[sizelimit_def] = { add1, add2, add3};

int LED_arr[] = {LED_PIN_MAIN,LED_PIN_GREEN_1,LED_PIN_GREEN_2,LED_PIN_GREEN_3};

//Allocate for storing current date and timec bad location
static String currDateTime;
static String gps_raw;
static float CurrLat;
static float CurrLong;

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
  BLEDevice::init("");
  delay(500);
  
  //Start serial communication
  Serial.begin(UART_BAUD);
  // Serial1.begin(UART_BAUD, SERIAL_8N1, BOARD_RX, BOARD_TX);
  // Serial2.begin(UART_BAUD, SERIAL_8N1, BOARD_RX, BOARD_TX);
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

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println(ClientMasterInit ? "ClientInited = true" : "ClientInited = false");
  if (!ClientMasterInit && digitalRead(StartLogButton) == HIGH){
    for (int i = 0 ; i < sizelimit_def ; i++){      
      Serial.println("[[CONNECT BEACON VIA PIN]]");
      if (digitalRead(StartLogButton) == HIGH && !AuthenticateServer(ServerAddressArray[i])) break;

      Serial.println("[[SCAN NEW DEVICE]]");
      pBLEScan->start(20, false);
    }
    if (MyClientServerManager->getNumberofClientServer() > 0){
      ClientMasterInit = true;
    }
  }

  else if (ClientMasterInit){
      Serial.println("[[SCAN SAVED DEVICE]]");
      pBLEScan->start(20, true);
  }
  else {
    Serial.print("Devices not initialised properly (No Device Added)");
    while(true){
      delay(500);
      Serial.print(".");}
  }
  
  connect_All();

  Serial.println("Connected");

  MyClientServerManager->checkAllConnected();
  MyClientServerManager->sendReadings();

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
 
  // Obtain a reference to the characteristics in the service of the remote BLE server.
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

  TotalConnected += 1;
  MyClientServerManager->TotalConnectedClientServer += 1;

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

void ClientServerManager::sendReadings() {
  Serial.println("@ClientServerManager::sendReadings: SerialPrint + Store + Post");  

  //Print Self Readings

  //Get Time
  std::string CurrentDateTime = "69/69/69,99:99:99+22"; // modem.getGSMDateTime(DATE_FULL);

  Serial.println("\t>> Checking that all ClientServer is Ready");
  while (!MyClientServerManager->checkAllReady()) {
    delay(500);
  }
  Serial.println("\t>> Sending Readings from all ClientServer");
  for (int i = 0; i < sizelimit; i++) {
    Serial.print("\t>> Next Server: ");
    if (MyClientServerManager->MyClientServerList[i]->IsConnected
      && MyClientServerManager->MyClientServerList[i]->IsInit
      && MyClientServerManager->MyClientServerList[i]->ReadyCheck.CharReady
      && MyClientServerManager->MyClientServerList[i]->ReadyCheck.HumReady
      && MyClientServerManager->MyClientServerList[i]->ReadyCheck.TempReady) 
    {    
      std::string SavedServer = MyClientServerManager->MyClientServerList[i]->pServerAddress->toString().c_str();

    Serial.printf("BLE Server : %s \n", SavedServer.c_str());
    Serial.printf("Readings: Temperature = %s C - ", MyClientServerManager->MyClientServerList[i]->NewData.NewTemp.c_str());
    Serial.printf("Humidity = %s %% - ", MyClientServerManager->MyClientServerList[i]->NewData.NewHum.c_str());
    Serial.printf("Date Time = %s \n", CurrentDateTime.c_str());
    MyClientServerManager->MyClientServerList[i]->ReadyCheck.HumReady == false;
    MyClientServerManager->MyClientServerList[i]->ReadyCheck.TempReady == false;

    //Store in SD

    //Send to Cloud}
    }
    else
    {
    Serial.println("BLE Server Unavailable/ Unready");
    }
  }
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

/////

