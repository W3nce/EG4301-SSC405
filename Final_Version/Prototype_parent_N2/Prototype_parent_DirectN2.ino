#include "Prototype_parent_DirectN2.h"
#include "utilities.h"
#include "debug_utils.h"
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


#define Child_1_ID          "A0:76:4E:44:44:B8"
#define Child_2_ID          "A0:76:4E:40:1B:78"
#define Child_3_ID          "00:00:00:00:00:00"

#define CurrShippingID      5

//Activate notify
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

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

RTC_DATA_ATTR static char add1[ServerAddressLength + 1] = Child_1_ID;
RTC_DATA_ATTR static char add2[ServerAddressLength + 1] = Child_2_ID;
RTC_DATA_ATTR static char add3[ServerAddressLength + 1] = Child_3_ID;
RTC_DATA_ATTR static char * ServerAddressArray[sizelimit_def] = { add1, add2, add3};

uint8_t LED_arr[] = {LED_PIN_MAIN,LED_PIN_GREEN_1,LED_PIN_GREEN_2,LED_PIN_GREEN_3};

//Allocate for storing current date and timec bad location
static String currDateTimePOST = "2023-01-01:00:00%2B00:00";
static String currDateTimeCSV = "2023-01-01:00:00+00:00";
RTC_DATA_ATTR static float CurrLat = 1.3002;
RTC_DATA_ATTR static float CurrLong = 103.7707;
static float CurrTemp_Cloud;
static float CurrHum_Cloud;
static unsigned int startTime;

//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {  

    const char * ServerFound = advertisedDevice.getName().c_str();
    bool connected = false;

    if (advertisedDevice.haveName() 
    && advertisedDevice.haveServiceUUID() 
    && advertisedDevice.getServiceUUID().equals(bmeServiceUUID) 
    && in_ServerAddressArray(ServerFound))
    { 
      //Check if the name of the advertiser matches
      Debugln("@AdvDevCallbacks:\tFound BLE Advertised Device ", true);  

      if(ClientMasterInit 
      ){
        BLEAdvertisedDevice* NewDev = new BLEAdvertisedDevice(advertisedDevice);

        Debugln("\t>> Adding Exisiting Device >> ", true);      
        connected = MyClientServerManager->addDevice(NewDev);
        Debug(connected?"Success":"Failed", true);    
      }

      else if (!ClientMasterInit 
      && (MyClientServerManager->contains(&advertisedDevice) == -1) 
      ){
        advertisedDevice.getScan()->stop();
        BLEAdvertisedDevice* NewDev = new BLEAdvertisedDevice(advertisedDevice);

        Debugln("\t>> Adding New Device >> ",true);
        connected = MyClientServerManager->addDevice(NewDev);
        Debug(connected?"Success":"Failed", true);        
      }
      
      else {
      Debugln(">> End CallBack:\tDevice UUID/Name Unmatch! ", TurnOnScanResults);
      return;
      }

      //Scan can be stopped, if all found
      if (MyClientServerManager->getNumberofClientServer() == sizelimit_def) {
        Debugln("\t>> Stopping Scan",true);
        advertisedDevice.getScan()->stop();
        Debugln(">> End CallBack:\tAll Device Added", true); 
        return;
      }
      return;
    }
  }  
  
};

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Debugln(">> ClientCallBack:\tonConnect",true);
  }

  void onDisconnect(BLEClient* pclient) {
    Debugln(">> ClientCallBack:\tonDisconnect",true);
    //MyClientServerManager->removeDevice(pclient);
  }
};

void setup(){
  unsigned int setupTime = millis();
  // Init BLE device
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  // Check LED
  InitLED();

  BLEDevice::init("");
  //Start serial communication
  Serial.begin(UART_BAUD);
  // Serial1.begin(UART_BAUD, SERIAL_8N1, BOARD_RX, BOARD_TX);
  Serial1.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println("Starting Arduino BLE Client application...");
  
  //Prints WakeUp Reason
  print_wakeup_reason();

  //Saves the current time in milliSeconds
  startTime = millis();
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  Serial.println(ClientMasterInit ? "ClientInited = true" : "ClientInited = false");
  bool states[] = {false,false,false,false};
  GetLEDState(states,sizelimit_def+1);

  //LOW => Stop pairing and start connecting
  //HIGH => Continue pairing  
  
  pinMode(StartLogButton,INPUT_PULLUP);
  if(digitalRead(StartLogButton) == LOW){
    Serial.println("[[SCAN EXISTING DEVICE]]");
    pBLEScan->start(20, true);
  }
  else if (!ClientMasterInit && digitalRead(StartLogButton) == HIGH){
    while(digitalRead(StartLogButton) == HIGH){
      
      Serial.println("[[SCAN NEW DEVICE]]");
      pBLEScan->start(5, false);
      states[MyClientServerManager->getNumberofClientServer()] = true;
      ToggleLEDState(states, sizelimit_def+1);
      if (MyClientServerManager->getNumberofClientServer() == sizelimit_def){
        ClientMasterInit = true;
      }
    }
  }
  Serial.println("[[SCANNING DONE]]");
  bool Connect_states[] = {true,false,false,false};
  ToggleLEDState(Connect_states, sizelimit_def+1);
  
  Serial.println("[[CONNECT ALL SAVED DEVICES]]");
  connect_All();
  Serial.println("[[CONNECTION COMPLETE]]");

  MyClientServerManager->checkAllConnected();
  Serial.println("[[CONNECTION CHECK COMPLETE]]");

  SDInit = SD_init(MyClientServerManager->TotalConnectedClientServer, SDInit);  
  Serial.println("[[SD INIT COMPLETE]]");
  /* PAIRING AND SD INITILISE DONE *////////////////////////////////////////////

  //Saves the current time in milliSeconds
  startTime = millis();
  Sensor_Start();
  Serial.println("\t>> Sensor Initialised, Start Reading ");
  getValues();
  Serial.println("[[START SENSOR & GET READING]]");
  printReading("BLE Parent", String(temp).c_str(), String(hum).c_str());

  int sendReadingsRes = MyClientServerManager->sendReadings(datacount);
  switch (sendReadingsRes){
    case 0:
      Serial.println("[[READINGS SENT]]");
      break;

    case 1:
      Serial.println("[[READINGS SKIPPED]]");
      break;

    case 2:
      Serial.println("[[READINGS FAILED TO SEND]]");
      break;

    default:
      Serial.println("[[READINGS GOING CRAZY]]");
      break;
  }

  if (MyClientServerManager->storeAllReadings(datacount)){
    Serial.println("[[READINGS STORED]]");    
  } 
  else {
    Serial.println("[[READINGS STORE ERROR]]");  
  }
  datacount++;

  //Start Deep Sleep for (TIME_TO_SLEEP - TIME_TAKEN_TO_RUN_PROGRAM)
  float TimeDiff = (float)TIME_TO_SLEEP - ((float)(millis() - startTime) / 1000) ;
  if (TimeDiff < 0) {
    Serial.println("Program took" + String(-TimeDiff)+
    "Seconds more than Sleep Time ("+String(TIME_TO_SLEEP)+"s)");    
    esp_sleep_enable_timer_wakeup(0);

  } else {
    uint64_t SleepTimeMS = TimeDiff * uS_TO_S_FACTOR;
    esp_sleep_enable_timer_wakeup(SleepTimeMS);

  }
  Serial.println("Setup ESP32 to sleep for " + String((TimeDiff<0?0:TimeDiff)) + " Seconds");
  Serial.flush();
  
  Serial.print(String(millis() - startTime).c_str());
  Serial.printf(" MilliSeconds Taken after Disconnect (Total %s MilliSeconds)\n", String(millis()-setupTime).c_str());
  Serial.println("[[GOING TO SLEEP]]");  
  esp_deep_sleep_start();
}

void loop(){
}


/* BLE Related functions */
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
        Serial.print(" Registered :\tFound ");
        Serial.println(tarserver.c_str());
        return true;
      }
      ind++;
    }
    return false;
  }

  void connect_All(){
    Debugln("@Connect_All:\tConnecting to all Server",true); 
    bool states[] = {false,false,false,false};
    GetLEDState(states,sizelimit_def+1);
    for (int i = 0 ; i < sizelimit_def ; i++){
      Flash(i+1,300);
      Debug(">> Connecting to Server ",TurnOnConnResults);
      Debugln(String(i + 1).c_str(),TurnOnConnResults);
      if (MyClientServerManager->MyClientServerList[i]->IsInit && !MyClientServerManager->MyClientServerList[i]->IsConnected){

        if(connect_Server(MyClientServerManager->MyClientServerList[i]->pClient, MyClientServerManager->MyClientServerList[i]->pAdvertisedDevice))
        {
          MyClientServerManager->MyClientServerList[i]->IsConnected = true;      
          MyClientServerManager->MyClientServerList[i]->ReadyCheck.CharReady = true;
          states[i+1] = true;
        };
        ToggleLEDState(states,sizelimit_def+1);
      }
    }
    Debug(">> End Server Connection:\tAll Connected - ",true);   
    Debugf("Total Device Connected:\t%d / ", MyClientServerManager->TotalConnectedClientServer,true); 
    Debugf("%d Devices \n", sizelimit_def, true) ;
  }

  bool connect_Server(BLEClient * client_test, BLEAdvertisedDevice * advdev_test){
    Debugln("\t>> Set and Create Client ",TurnOnConnResults);
    client_test = BLEDevice::createClient();
    client_test->setClientCallbacks(new MyClientCallback()); 

    Debugln("\t>> Connect to server ",TurnOnConnResults); 
    client_test->connect(advdev_test); 

    Debugln("\t>> Get BME Service",TurnOnConnResults); 
    BLERemoteService* pRemoteService = client_test->getService(bmeServiceUUID);
    if (pRemoteService == nullptr) {
      Debug("\t\t>>Failed to find our service UUID:\t",TurnOnConnResults);
      Debugln(bmeServiceUUID.toString().c_str(),TurnOnConnResults);
      Debugln(">> End Server Connection:\tFail BME Service not found",true);   
      return (false);
    }
  
    // Obtain a reference to the characteristics in the service of the remote BLE  server.
    Debugln("\t>> Get Characteristics",TurnOnConnResults); 
    BLERemoteCharacteristic* TempChar = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
    BLERemoteCharacteristic* HumChar = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

    if (TempChar == nullptr || HumChar == nullptr) {
      Debugln("\t\t>> Failed to find our characteristic UUID",TurnOnConnResults);
      Debugln(">> End Server Connection:\tFail Characteristic not found",true); 
      return false;
    }
  
    Debugln("\t>> Assign Notification Callback Functions",TurnOnConnResults); 
    //Assign callback functions for the Characteristics
    TempChar->registerForNotify(temperatureNotifyCallback);
    HumChar->registerForNotify(humidityNotifyCallback);

    
    Debugln("\t>> Register Characteristics for Notify",TurnOnConnResults); 
    //Activate the Notify property of each Characteristic
    TempChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
    HumChar->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);

    MyClientServerManager->TotalConnectedClientServer = MyClientServerManager->TotalConnectedClientServer + 1;

    Debugln("\t>> End Server Connection:\tServer connected",true); 
    return true;
  }
////////////////////////////////////////////////////////////////////////////////

/* Internal ClientServer and ClientServerManager functions */
  ClientServer::ClientServer(){
    Serial.println("@ClientServer::ClientServer:\tInitialising ClientServer Object ");    
    Serial.println(">> End Initialisation"); 
  }

  void ClientServer::setDevice(BLEAdvertisedDevice *p_AdvertisedDevice){
    Serial.println("@ClientServer::setDevice:\tSetting Device"); 
    Serial.println("\t>> Set Adv Device ");     
    pAdvertisedDevice = p_AdvertisedDevice;
    Serial.println("\t>> Set BLE Address ");     
    pServerAddress = new BLEAddress(p_AdvertisedDevice->getAddress());
    
    Serial.println(">> End Device Set");    
  }

  ClientServerManager::ClientServerManager(){
    Serial.println("@ClientServerManager::ClientServerManager:\tInitialising"); 
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
    Debugln("@ClientServerManager::addDevice:\tAdding Device", TurnOnScanResults);

    for (int i = 0 ; i < sizelimit ; i++){
      if (!MyClientServerList[i]->IsInit){
        Debugln("\t>> Found and setDevice for Client ", TurnOnScanResults);  
        Debug(i+1, TurnOnScanResults);
        Debugln(" with pAdvDev ", TurnOnScanResults);  
        MyClientServerList[i]->setDevice(pAdvDev);
        MyClientServerList[i]->IsInit = true;

        strcpy(ServerAddressArray[i], pAdvDev->getAddress().toString().c_str());

        TotalClientServer = TotalClientServer + 1;

        Debug("\t>> Added ", TurnOnScanResults);
        Debugf(">> %s - ", pAdvDev->getName().c_str(), TurnOnScanResults);
        Debugf("%s\n", pAdvDev->getAddress().toString().c_str(), TurnOnScanResults);

        Debug(">> End addDevice:\tSuccess - ", TurnOnScanResults); 
        Debugf("Total Device Added:\t%d / ", TotalClientServer, TurnOnScanResults);
        Debugf("%d Devices \n", sizelimit_def, true);

        return true;
      }
    }
    return false;
    Debugln(">> End addDevice:\tFail",true); 
  }

  int ClientServerManager::contains(BLEAdvertisedDevice* AdvDev) { 

    Debugln("@ClientServerManager::contains(BLEAdvDevice) ",TurnOnScanResults);
    for (int i = 0; i < sizelimit_def; i++) {
      if (MyClientServerList[i]->IsInit){
        BLEAddress OtherServer = *MyClientServerList[i]->pServerAddress;

        if (AdvDev->getAddress().equals(OtherServer)) {
          Debugln("\t>> BLEAdvDevice Found:\ttrue ",TurnOnScanResults);
          return i;
        }
      }    
    }
    
    Debugln(">> End contains(BLEAdvDevice):\tBLEAdvDevice not Found:\tfalse ",TurnOnScanResults);
    return -1;
  }

  bool ClientServerManager::checkAllConnected() {
    Serial.println("@ClientServerManager::checkAllConnected:\tChecking if All Server Connected");
    Serial.print("\t>> ServerCheck: ");
    if (sizelimit_def == TotalClientServer && TotalConnectedClientServer == TotalClientServer) {
      Serial.printf(">> End checkAllConnected: All Server Added & Connected: %d Connected/ %d Added Devices (Max %d) \n", TotalConnectedClientServer,TotalClientServer, sizelimit_def);
      return true;
    } else if (sizelimit_def != TotalClientServer && TotalConnectedClientServer == TotalClientServer){
      Serial.printf(">> End checkAllConnected: All Server Connected, Not All Added: %d Connected/ %d Added Devices (Max %d) \n", TotalConnectedClientServer,TotalClientServer, sizelimit_def);
      return false;
    } else if (sizelimit_def != TotalClientServer && TotalConnectedClientServer != TotalClientServer){
      Serial.printf(">> End checkAllConnected: Not All Server Connected, Not All Added: %d Connected/ %d Added Devices (Max %d) \n", TotalConnectedClientServer,TotalClientServer, sizelimit_def);
      return false;
    } else if (sizelimit_def == TotalClientServer && TotalConnectedClientServer != TotalClientServer){
      Serial.printf(">> End checkAllConnected: Not All Server Connected, All Added: %d Connected/ %d Added Devices (Max %d) \n", TotalConnectedClientServer,TotalClientServer, sizelimit_def);
      return false;
    } 
    Serial.println("");
  }

  bool ClientServerManager::checkAllReady() {
    Serial.println("@ClientServerManager::checkAllReady:\tChecking if All Characteristics Ready");
    int count = 0;
    if (MyClientServerManager->getNumberofClientServer() == 0) {
      Serial.println("No Server to check!");
      return false;
    }
    for (int i = 0; i < sizelimit; i++) {
      Serial.print("\t>> Next Server:\t");
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

        Serial.printf("Checking Target ClientServer (%s) :\t", SavedServer.c_str());
        Serial.println("Success:\tReady!");
        count += 1;
      } else {
        Serial.println("Failed:\tNot Added/ Not Ready");
      }
    }
    return count == MyClientServerManager->TotalConnectedClientServer;
  }

  void ClientServerManager::setTempChar(std::string Server, std::string Value){
    // Method/function defined inside the class
    Debugf("@ClientServerManager::setTempChar:\tSetting Temp Char for Server(%s):\t\n", Server.c_str(), TurnOnNotiResults);
    if (MyClientServerManager->getNumberofClientServer() == 0) {
      Debugln("No Server to set!", TurnOnNotiResults);
      return;
    }
    for (int i = 0; i < sizelimit; i++) {
      Debug("\t>> Next Server:\t", TurnOnNotiResults);
      std::string SavedServer = MyClientServerManager->MyClientServerList[i]->pServerAddress->toString();

      Debugf("Checking (%s) :\t", SavedServer.c_str(), TurnOnNotiResults);

      if (strcmp(SavedServer.c_str(), Server.c_str()) == 0) {
        if (MyClientServerManager->MyClientServerList[i]->ReadyCheck.TempReady){
          return;
        } 
        else{
          MyClientServerManager->MyClientServerList[i]->NewData.NewTemp = Value;
          MyClientServerManager->MyClientServerList[i]->ReadyCheck.TempReady = true;
          Debugln("Success:\tTempChar Set for Print", true);
          return;
        }
      } else {
        Debugln("Failed:\tServer Address Unmatch", TurnOnNotiResults);
      }
    }
    return;
  }

  void ClientServerManager::setHumChar(std::string Server, std::string Value){  
    // Method/function defined inside the class
    Debugf("@ClientServerManager::setHumChar:\tSetting Hum Char for Server(%s):\t\n", Server.c_str(), TurnOnNotiResults);

    if (MyClientServerManager->getNumberofClientServer() == 0) {
      Debugln("No Server to set!", TurnOnNotiResults);
      return;
    }

    for (int i = 0; i < sizelimit; i++) {
      Debug("\t>> Next Server:\t", TurnOnNotiResults);
      std::string SavedServer =  MyClientServerManager->MyClientServerList[i]->pServerAddress->toString();
      Debugf("Checking (%s) :\t", SavedServer.c_str(), TurnOnNotiResults);
      if (strcmp(SavedServer.c_str(), Server.c_str()) == 0) {
        if (MyClientServerManager->MyClientServerList[i]->ReadyCheck.HumReady){
          return;
        } else {
          MyClientServerManager->MyClientServerList[i]->NewData.NewHum = Value;
          MyClientServerManager->MyClientServerList[i]->ReadyCheck.HumReady = true;
          Debugln("Success:\tHumChar Set for Print", true);
          return;
        }
      } else {
        Debugln("Failed:\tServer Address Unmatch", TurnOnNotiResults);
      }
    }
    return;
  }

  int ClientServerManager::sendReadings(int &currdatacount ) {
    Serial.println("@ClientServerManager::sendReadings:\tCheck and Post All");  
    // 0 == OK
    // 1 == SKIP
    // 2 == FAIL
    int status = 2;

    Serial.println("\t>> Checking that all ClientServer is Ready");
    int checkStart = millis();
    while (!MyClientServerManager->checkAllReady() && (millis()-checkStart)<1550) {
      Serial.println("\t\t>> Not All Ready");
      delay(500);
    }
    
    Serial.println("\t>> Disconnecting BLE");
    BLEDevice::deinit(true);

    AverageReadings();

    // Start Modem and GPRS, Get GPS and Datetime, Post to Cloud
    // Serial.println("(int)(currdatacount-dataindex_last + 1)");
    // Serial.println((int)(currdatacount-dataindex_last + 1));
    // Serial.println("(int)(currdatacount-dataindex_last + 1)%READING_BEFORE_SEND)");
    // Serial.println((int)(currdatacount-dataindex_last + 1)%READING_BEFORE_SEND);

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
        Serial.println("\t>> GRPS and GPS Connection Success");

        Serial.println("\t>> Retrieving GPS Information");
        if (GetGPS()){
          Serial.println("\t\t>> Get GPS OK");
        }else {
          Serial.println("\t\t>> Get GPS TIMEOUT");
        };
        
        if(!GetTime_TinyGSM()){
          GetNextTime();
        }
        Serial.println("\t>> TinyGSM Client POST ALL to Sussy");
        if (!SendAllPostHttpRequest_TinyGSM(currdatacount)){
          Serial.println("\t>> Post Requests Failed");
          status = 2;
        }
        else {
          Serial.println("\t>> Post Requests Success");
          status = 0;
        };
        //dataindex_last++ occurs in PostSendAll
      }
      else {
        Serial.println("\t>> GRPS Connection Failed");
        GetNextTime();
        status = 2;
      };
      Serial.println("\t>> Stopping SIM Module and network functions");
      StopModem();
      Serial.println("End sendReadings:\tReadings Remaining :\t" + String(currdatacount - dataindex_last + 1));  
      return status;
    }
    else {
      Serial.println("\t>> GetTime due to skipping");
      GetNextTime();
      return 1;
    }
    Serial.println("End sendReadings:\tReadings Skipped :\t" + String(currdatacount - dataindex_last + 1));  
  }

  bool ClientServerManager::storeAllReadings(int &currdatacount ) {
    bool status = true;
    Serial.println("@ClientServerManager::storeAllReadings:\tStore All");  

    Serial.println("\t>> Storing Self and Averaged Readings in CSV");
    status = storeReading("/data.csv", currdatacount, temp, hum, currDateTimeCSV, CurrLat , CurrLong) ? status : false;
    status = storeReading("/avrg.csv", currdatacount, CurrTemp_Cloud, CurrHum_Cloud, currDateTimePOST, CurrLat , CurrLong) ? status : false;

    Serial.println("\t>> Storing Readings from all ClientServer");

    char * DataCSVPath[] = {"/data1.csv","/data2.csv","/data3.csv"};
    
    for (int i = 0; i < MyClientServerManager->TotalConnectedClientServer;i++) {
      Serial.print("\t>> Next Server:\t");
      ClientServer * CurrServer = MyClientServerManager->MyClientServerList[i];
      if (CurrServer->IsConnected && CurrServer->IsInit 
      && CurrServer->ReadyCheck.CharReady && CurrServer->ReadyCheck.HumReady 
      && CurrServer->ReadyCheck.TempReady) {

        std::string SavedServer = CurrServer->pServerAddress->toString().c_str();
        printReading(SavedServer.c_str(), String(temp).c_str(), String(hum).c_str());
        
        //Store in SD
        status = storeReading(DataCSVPath[i], currdatacount, CurrServer->NewData.NewTemp.c_str(), CurrServer->NewData.NewHum.c_str(),currDateTimeCSV, CurrLat , CurrLong)?status : false;
        
        CurrServer->ReadyCheck.HumReady = false;
        CurrServer->ReadyCheck.TempReady = false;
      }
      else {
        Serial.println("BLE Server Unavailable/ Unready"); 
        status = false;
      }
    }
    Serial.println("End checkReadings:\tAll Readings Saved");  
    return status;
  }
////////////////////////////////////////////////////////////////////////////////

/* Notification CallBack Functions */
  static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {

    Debug("[[Notification (TEMP)]] @ ",true);
    std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();

    Debugf(" from Server (%s) Notified ", ServerAddressStr.c_str(),true);
    char* temperatureChar = (char*)pData;
    temperatureChar[6] = '\0';
    std::string TempString = temperatureChar;
    Debugf("Temperature:\t%s C \n", TempString.c_str(),true);

    Debug("-> Call setTempChar:\t",TurnOnNotiResults);
    MyClientServerManager->setTempChar(ServerAddressStr, TempString);
  }
  //When the BLE Server sends a new humidity reading with the notify property
  static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,uint8_t* pData, size_t length, bool isNotify) {

    Debug("[[Notification (HUM)]] @ ",true);
    std::string ServerAddressStr = pBLERemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().toString();

    Debugf(" from Server (%s) Notified ", ServerAddressStr.c_str(),true);
    char* humidityChar = (char*)pData;
    humidityChar[6] = '\0';
    std::string HumString = humidityChar;
    Debugf("Humidity:\t%s %% \n", HumString.c_str(),true);
    Debug("-> Call setHumChar:\t",TurnOnNotiResults);
    MyClientServerManager->setHumChar(ServerAddressStr, HumString);
  }
////////////////////////////////////////////////////////////////////////////////

/* Reading functions */
  bool storeReading(const char* filename, int data_count, float curr_Temp, float curr_Hum, String curr_DateTime, float curr_Lat, float curr_Lon){
    //Store Self reading in SD
    String dataString = String(data_count) + "," 
    + String(curr_Temp) + "," + String(curr_Hum) + "," 
    + curr_DateTime + "," 
    + String(curr_Lat,8) + "," + String(curr_Lon,8);
    Serial.print("\t>> Appending :\t" + dataString + " >> ");

    if (SDInit && writeDataLine(SD, filename, dataString.c_str())) {
      Serial.println("\t>> New Data added");
      // readFile(SD, filename);
      return true;
    } else {
      Serial.println("\t>> New Data not added");
      return false;
    }
  }

  bool storeReading(const char* filename, int data_count, const char* curr_Temp, const char* curr_Hum, String curr_DateTime, float curr_Lat, float curr_Lon){
    //Store Self reading in SD
    String dataString = String(data_count) + "," 
    + String(curr_Temp) + "," + String(curr_Hum) + "," 
    + curr_DateTime + "," 
    + String(curr_Lat,8) + "," + String(curr_Lon,8);
    Serial.print("\t>> Appending :\t" + dataString + " >> ");

    if (SDInit && writeDataLine(SD, filename, dataString.c_str())) {
      Serial.println("\t>> New Data added");
      // readFile(SD, filename);
      return true;
    } else {
      Serial.println("\t>> New Data not added");
      return false;
    }
  }

  void printReading(const char* savedServer, const char* curr_Temp, const char* curr_Hum){
        Serial.printf("\t>> BLE Device :\t%s \n", savedServer);
        Serial.printf("\t>> Readings:\tTemperature =\t%s C", curr_Temp);
        Serial.printf("\tHumidity =\t%s %% \n",curr_Hum);
  }

  void AverageReadings(){
    Serial.println(">> AverageReadings :\tCollate and Average Self and Beacon Readings");
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
    Serial.println("\t>> Total Readings to Average:\t" + String(TotalCurrReadings));
    Serial.println("\t>> Avg Temp :\t" + String(CurrTemp_Cloud) + " - " + "Avg Hum :\t" + String(CurrHum_Cloud));
    Serial.println(">> End AverageReadings :\tReadings Averaged");
  }

  void AddHours( struct tm* date) {
      // const time_t DEEP_SLEEP_TIME = HRS_TO_SLEEP * 60 * 60 ;
      const time_t DEEP_SLEEP_TIME = TIME_TO_SLEEP ;
      const time_t p_date_seconds = mktime( date )  ;
      Serial.println("\t>> Previous DateTime:\t" + String(ctime( &p_date_seconds ))); 

      // Seconds since start of epoch
      const time_t date_seconds = mktime( date ) + (DEEP_SLEEP_TIME) ;
      Serial.println("\t>> Current DateTime:\t" + String(ctime(&date_seconds)));
      
      // Update caller's date
      // Use localtime because mktime converts to UTC so may change date
      *date = *localtime( &date_seconds ) ; ;
  }

  void GetNextTime(){ 
    Serial.println(">> GetNextTime :\tAdd Sleep time manually to last saved time");
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

    Serial.println("End GetNextTime :\tDateTime adjusted");
  }
////////////////////////////////////////////////////////////////////////////////

/* LILYGO/TinyGSM Commands */
  void TurnOnSIMModule(){
    Debugln("\t>> TinyGSM:\tTurning on SIM Module",true);
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
    Debugln("@TingyGSM::StartModem:\tInitializing modem",true);
    modem.init();
    light_sleep(1);
    if (!modem.init()) {
      Debugln("\t>> Failed to start modem, delaying 2s and retrying",true);
      delay(100);
      light_sleep(2);
    }
    Debugln("End StartModem:\tInitialize OK",true);
  }

  void StopModem() {
    Debugln("@TingyGSM::StopModem:\tStopping GPRS and modem",true);
    modem.gprsDisconnect();
    delay(200);
    if (!modem.isGprsConnected()) {
      Debugln("\t>> GPRS disconnected",true);
    } else {
      Debugln("\t>> GPRS disconnect:\tFailed",true);
    }
    modem.sleepEnable();
    delay(100);
    modem.poweroff();
    // pinMode(4, INPUT_PULLUP);
    Debugln("End StopModem:\tStopped GPRS and modem",true);
  }

  bool StartGPRS() {
    Debugln("@TingyGSM::StartGPRS:\tStarting GPRS Service", true);
    // Set Modem to Auto, Set GNSS
    /*  Preferred mode selection : AT+CNMP
      2 – Automatic

      do {
        ret = modem.setNetworkMode(2);
        delay(500);
      } while (ret != "OK");
    */

    bool ret;
    ret = modem.setNetworkMode(2);
    modem.waitResponse(GSM_OK);
    Debugf("\t\>> setNetworkMode:\t%s", ret? "OK" : "FAIL", TurnOnSIMResults);

    /**
      https://github.com/vshymanskyy/TinyGSM/pull/405
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
    uint8_t mode = modem.getGNSSMode();
    Debugf("\tGNSS Mode:\t%s\n", String(mode).c_str(), TurnOnSIMResults);

    String name = modem.getModemName();
    Debugf("\t>> Modem Name:\t%s" , name.c_str(), TurnOnSIMResults);

    String modemInfo = modem.getModemInfo();
    Debugf("\tModem Info:\t%s\n" , modemInfo.c_str(), TurnOnSIMResults);

    // Unlock your SIM card with a PIN if needed
    if (GSM_PIN && modem.getSimStatus() != 3) {
      modem.simUnlock(GSM_PIN);
    }

    Debugln("\t>> Waiting for network...", TurnOnSIMResults);
    DBG("Waiting for network...");
    modem.waitForNetwork(10000L);
    light_sleep(1);

    if (modem.isNetworkConnected()) {
      Debugln("\t>> Network connected", true);
    } else {
      Debugln("\t>> Network not connected", true);
      return false;
    }
      
    // Connect to APN
    Debugf("\t>> APN Connected:\t%s", APN, true);
    // Debugln(APN,true);
    DBG("Connecting to", APN);
    modem.gprsConnect(APN, GPRS_USER, GPRS_PASS);
    light_sleep(1);

    //ret will represent the modem connection's success/ failure
    ret = modem.isGprsConnected();
    Debugf("\tGPRS status:\t%s\n", String(ret?"connected":"not connected").c_str(), true);
    if (!ret) return false;

    String ccid = modem.getSimCCID();
    Debugf("\t>> CCID:\t%s", ccid.c_str(), TurnOnSIMResults);

    String imei = modem.getIMEI();
    Debugf("\tIMEI:\t%s\n",  imei.c_str(), TurnOnSIMResults);

    String imsi = modem.getIMSI();
    Debugf("\t>> IMSI:\t%s", imsi.c_str(), TurnOnSIMResults);

    String cop = modem.getOperator();
    Debugf("\tOperator:\t%s\n", cop.c_str(), TurnOnSIMResults);

    IPAddress local = modem.localIP();
    Debugf("\t>> Local IP:\t%s", local.toString().c_str(), TurnOnSIMResults);

    int csq = modem.getSignalQuality();
    Debugf("\tSignal Quality:\t%s\n", String(csq).c_str(), TurnOnSIMResults);
    Debugln("End StartGPRS:\tGPRS Started",true);
    return true;
  }

  void SendPostHttpRequest_TinyGSM() {
    Serial.println("@TingyGSM::SendPostHttpRequest_TinyGSM:\tStarting Post Request");

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
    Serial.print("\t>> Posting:\t");
    Serial.println(postData);

    http.beginRequest();
    int err = http.post("/api/master/updateShipment");
    Serial.println("\t>> Error Code:\t" + String(err));
    http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
    http.sendHeader("Content-Length", postData.length());
    http.beginBody();
    http.print(postData);
    http.endRequest();

    int status = http.responseStatusCode();
    Serial.print("\t>> Response status code:\t"+String(status));
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
    //   Serial.println("\t>> Content length is:\t" + String(length));
    // }
    // if (http.isResponseChunked()) {
    //   Serial.println("\t>> The response is chunked");
    // }

    // String body = http.responseBody();
    // Serial.println("\t>> Response:" + body);

    // Serial.print("\t>> Body length is:\t");
    // Serial.println(body.length());

    http.stop();
    Serial.println("\t>> Server disconnected");
    Serial.println("End SendPostHttpRequest_TinyGSM:\tPost request success");
  }

  bool SendAllPostHttpRequest_TinyGSM(int &currdatacount) {
    Debugln("@TingyGSM::SendAllPostHttpRequest_TinyGSM:\tStarting Post Request for All Devices",true);

    const char server[] = "sussylogger-2.fly.dev";  
    const char URLPath[] = "/api/master/updateShipment";
    const int port = 80;
    int shipmentID = CurrShippingID;
    int currindex = -1;
    float Temp_Cloud;
    float Hum_Cloud;
    char DateTime_Cloud[27+1]  , cptr[100];
    float Lat_Cloud;
    float Long_Cloud;
    int PostRetries = 0;

    Debugln("\t>> Creating HttpClient", TurnOnSIMResults);
    TinyGsmClient client(modem);
    HttpClient http = HttpClient(client, server, port);

    Debugln("\t>> Reading from avrg.csv", TurnOnSIMResults);
    File file = SD.open("/avrg.csv");
    readLine(file, cptr ,100);
    if (TurnOnSIMResults){
      Serial.print("\t\t>> Headers:\t"); 
      Serial.println(cptr);
    }
    while(currindex < dataindex_last - 1){      
      readLine(file, cptr ,100);
      if (TurnOnSIMResults){
        Serial.print("\t\t>> Past Reading:\t"); 
        Serial.println(cptr);
      }
      currindex = std::atoi(cptr);
    }

    Serial.println("\t>> Posting Unsent Readings");
    while(dataindex_last < currdatacount ){
      Serial.print("\t>> Curr Datacount:\t"+ String(currdatacount) + 
            "\tLast Unsent:\t" + String(dataindex_last));

      Serial.print("\tReading Idx:\t" + String(currindex+1));
      if (readVals(file,&currindex,&Temp_Cloud,&Hum_Cloud,DateTime_Cloud,27,&Lat_Cloud,&Long_Cloud)){
        Serial.println("\tStatus:\tOK");
      }else{
        Serial.println("\tStatus:\tFAIL");
        break;
      };
      DateTime_Cloud[27] = '\0';
      PostRetries = 0;
      while (PostRetries < 3){
        Serial.println("\t>> Posting Idx:\t\t" + String(currindex) + "\tTries:\t\t" + String(PostRetries+1));
        Serial.println("\t>> Posting to:\t" + String(server) + "\tPath:\t" + String(URLPath));
        // SendHttpRequest(&http, URLPath, shipmentID, Temp_Cloud, Hum_Cloud, DateTime_Cloud, Lat_Cloud, Long_Cloud);

        String contentType = "application/x-www-form-urlencoded";
        String postData =   "shipmentID=" + String(shipmentID) 
                          // + "&childid=" + childName 
                          + "&temperature=" + String(Temp_Cloud) 
                          + "&humidity=" + String(Hum_Cloud) 
                          + "&latitude=" + String(Lat_Cloud,8)
                          + "&nS=N"
                          + "&longitude=" + String(Long_Cloud,8) 
                          + "&eW=E"
                          + "&DateTime=" + String(DateTime_Cloud);

        Serial.println("\t\t>> Post Body:\t" + postData);

        http.beginRequest();
        int err = http.post("/api/master/updateShipment");
        Serial.print("\t\t>> Error Code:\t" + String(err));
        http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
        http.sendHeader("Content-Length", postData.length());
        http.beginBody();
        http.print(postData);
        http.endRequest();

        int status = http.responseStatusCode();
        Serial.println("\tResponse status code:\t" + String(status));
        PostRetries++;
        if (status == 200){
          dataindex_last++;
          break;
        }
        if (PostRetries == 2){
          Serial.println("End SendAllPostHttpRequest_TinyGSM:\tPost request Failed");
          return false;
        }
      }
    }

    file.close();

    if (currdatacount==dataindex_last) {
      Serial.println("\t>> Curr Datacount:\t"+ String(currdatacount) + 
            "\tLast Unsent:\t" + String(dataindex_last));
      PostRetries = 0;
      while(PostRetries<3){
        Serial.println("\t>> Posting Idx:\t\t" + String(currdatacount) + "\tTries:\t\t" + String(PostRetries+1));
        Serial.println("\t>> Posting to:\t" + String(server) + "\tPath:\t" + String(URLPath));
        // SendHttpRequest(&http, URLPath, shipmentID, CurrTemp_Cloud, CurrHum_Cloud, currDateTimePOST, CurrLat, CurrLong);

        String contentType = "application/x-www-form-urlencoded";
        String postData =   "shipmentID=" + String(shipmentID) 
                          // + "&childid=" + childName 
                          + "&temperature=" + String(CurrTemp_Cloud) 
                          + "&humidity=" + String(CurrHum_Cloud) 
                          + "&latitude=" + String(CurrLat,8)
                          + "&nS=N" 
                          + "&longitude=" + String(CurrLong,8) 
                          + "&eW=E"
                          + "&DateTime=" + currDateTimePOST;

        Serial.println("\t\t>> Post Body:\t" + postData);

        http.beginRequest();
        int err = http.post("/api/master/updateShipment");
        Serial.print("\t\t>> Error Code:\t" + String(err));
        http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
        http.sendHeader("Content-Length", postData.length());
        http.beginBody();
        http.print(postData);
        http.endRequest();

        int status = http.responseStatusCode();
        Serial.println("\tResponse status code:\t" + String(status));
        PostRetries++;
        if (status == 200){
          dataindex_last++;
          return true;
        }
        if (PostRetries == 2){
          Serial.println("End SendAllPostHttpRequest_TinyGSM:\tPost request Failed");
          return false;
        }
      }
    }

    http.stop();
    Serial.println("End SendAllPostHttpRequest_TinyGSM:\tPost request Done and Server disconnected");
  }

  void SendGetHttpRequest_TinyGSM() {

    // const char server[] = "sussylogger-2.fly.dev";  //
    // const char resource[] = "/";
    // const int port = 80;

    // TinyGsmClient client(modem);
    // HttpClient http(client, server, port);
    // Serial.print(F("Performing HTTP GET request to ... "));
    // Serial.print(F(server));
    // // http.connectionKeepAlive();  // Currently, this is needed for HTTPS
    // int err = http.get(resource);
    // if (err != 0) {
    //   Serial.println(F(" failed to connect"));
    //   delay(10000);
    //   // return;
    // }
    // Serial.println(" ");

    // int status = http.responseStatusCode();
    // Serial.print(F("Response status code:\t"));
    // Serial.println(status);
    // if (!status) {
    //   delay(10000);
    //   // return;
    // }

    // Serial.println(F("Response Headers:"));
    // if (http.headerAvailable()) {
    //   String headerName = http.readHeaderName();
    //   String headerValue = http.readHeaderValue();
    //   Serial.println("    " + headerName + " : " + headerValue);
    // }

    // int length = http.contentLength();
    // if (length >= 0) {
    //   Serial.print(F("Content length is:\t"));
    //   Serial.println(length);
    // }
    // if (http.isResponseChunked()) {
    //   Serial.println(F("The response is chunked"));
    // }

    // String body = http.responseBody();
    // Serial.println(F("Response:"));
    // Serial.println(body);

    // Serial.print(F("Body length is:\t"));
    // Serial.println(body.length());

    // // Shutdown

    // http.stop();
    // Serial.println(F("Server disconnected"));
  }

  bool GetTime_TinyGSM() {
    Debugln("@TingyGSM::GetTime_TinyGSM:\tGetting Network Time",true);
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    float tz = 0;
    for (int8_t i = 5; i; i--) {
      DBG("Requesting current network time");
      if (modem.getNetworkTime(&year, &month, &day, &hour, &min, &sec, &tz)) {
        break;
      } 
      else {
        DBG("Couldn't get network time");
        Debugln("End GetTime_TinyGSM:\tFailed to geNetwork Time", true);
        return false;
      }
    }
    LastYr = year;
    LastMth = month;
    LastDay = day;
    LastHr = hour;
    LastMin = min;
    LastSec = sec;
    LastTZ = tz;
    String datefull = modem.getGSMDateTime(DATE_FULL); //23/03/03,00:39:54+32
    String datetime = modem.getGSMDateTime(DATE_TIME); //00:39:54+32
    // String datedate = modem.getGSMDateTime(DATE_DATE); //23/03/03
    // Serial.println("Current Network Date_Full:\t" + datefull) ;
    // Serial.println("Current Network Date_Time:\t" + datetime);
    // Serial.println("Current Network Date_Date:\t" + datedate);

    currDateTimePOST = String(year) + "-" + datefull.substring(3,5) + "-" + datefull.substring(6,8) + "T" + datetime.substring(0,2) + ":" + datetime.substring(3,5) + ":" + datetime.substring(6,8) + toTimezoneString(tz);

    currDateTimeCSV = String(year) + "-" + datefull.substring(3,5) + "-" + datefull.substring(6,8) + "T" + datetime.substring(0,2) + ":" + datetime.substring(3,5) + ":" + datetime.substring(6,8) + toTimezoneStringCSV(tz);

    currDateTimePOST = currDateTimePOST.c_str();
    Debugf("End GetTime_TinyGSM:\tNetwork Time:\t%s\n", currDateTimeCSV, true);
    return true;
  }

  bool GetGPS() {
    bool states[] = {false,false,false,false};
    GetLEDState(states,sizelimit_def+1);
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
    const uint8_t GPSstart = millis();
    while((millis() - GPSstart)< 60000){
      if (modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                      &year2, &month2, &day2, &hour2, &min2, &sec2)) {
        DBG("Latitude:", String(lat2, 8), "\tLongitude:", String(lon2, 8));
        DBG("Speed:", speed2, "\tAltitude:", alt2);
        DBG("Visible Satellites:", vsat2, "\tUsed Satellites:", usat2);
        DBG("Accuracy:", accuracy2);
        DBG("Year:", year2, "\tMonth:", month2, "\tDay:", day2);
        DBG("Hour:", hour2, "\tMinute:", min2, "\tSecond:", sec2);
        CurrLat = lat2;
        CurrLong = lon2;
        Serial.println("Saved GPS Lat and Long:\t"+String(CurrLat,8) + ","+String(CurrLong,8));
        DBG("Retrieving GPS/GNSS/GLONASS location again as a string");
        static String gps_raw = modem.getGPSraw();
        
        DBG("GPS/GNSS Based Location String:", gps_raw);
        Serial.print("GPS/GNSS Based Location String:\t");
        Serial.println(gps_raw);
        DBG("Disabling GPS");
        Serial.println("Disabling GPS");
        modem.disableGPS();
        return true;
      } 
      else {
        if (TurnOnSIMResults){
          Loading();
        } else {
          light_sleep(2);
        }
        Debugln("Retrying in 2 Seconds",TurnOnSIMResults);
      }
    }
    ToggleLEDState(states,sizelimit_def+1);
    DBG("Disabling GPS");
    Serial.println("Disabling GPS");
    modem.disableGPS();
    return false;
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


/* MISC */

  void InitLED(){
    for (int ind = 0; ind <= sizelimit_def ;ind++){
      pinMode(LED_arr[ind], OUTPUT);
      digitalWrite(LED_arr[ind], HIGH);
      delay(150);
    };
    delay(200);
    OffAll();
    delay(200);
    OnAll();
    delay(200);
    bool temp[] = {true ,false ,false, false};
    ToggleLEDState(temp,sizelimit_def+1);

  }

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
  //1,2,3
  void Flash(int LED_NO, int del){
    if (LED_NO>sizelimit_def){
      Serial.println("Invalid LED_NO (0,1,2,3 Only)");
      return;
    }

    digitalWrite(LED_arr[LED_NO],digitalRead(LED_arr[LED_NO])?LOW : HIGH); 
    delay(del);
    digitalWrite(LED_arr[LED_NO],digitalRead(LED_arr[LED_NO])?LOW : HIGH); 
  }

  void Loading(){
    OffAll();
    for (int ind = 0; ind < sizelimit_def ;ind++){
      Flash(ind,330);
    };
      for (int ind = sizelimit_def; ind >0 ;ind--){
      Flash(ind,330);
    };
  }

  void ToggleLEDState(bool states[],int len){
    if (len>sizelimit_def + 1){
      Serial.println("Invalid length (max: 4)");
      return;
    }
    for (int ind = 0; ind < len ;ind++){
      digitalWrite(LED_arr[ind], states[ind]?HIGH:LOW);
    };
  }
  
  void GetLEDState(bool states[],int len){
    if (len>sizelimit_def + 1){
      Serial.println("Invalid length (max: 4)");
      return;
    }
    for (int ind = 0; ind < len ;ind++){
      states[ind] = digitalRead(LED_arr[ind]);
    };
  }

  void Debugf(const char* main, const char* format, bool bswitch) {
    if (bswitch) {
      Serial.printf(main, format);
    }
  }

  void Debugf(const char* main, int format, bool bswitch) {
    if (bswitch) {
      Serial.printf(main, format);
    }
  }

  void Debugf(const char* main, float format, bool bswitch) {
    if (bswitch) {
      Serial.printf(main, format);
    }
  }

  void Debugf(const char* main, String format, bool bswitch) {
    if (bswitch) {
      Serial.printf(main, format.c_str());
    }
  }

  void Debug(const char* main, bool bswitch) {
    if (bswitch) {
      Serial.print(main);
    }
  }

  void Debug(int main, bool bswitch) {
    if (bswitch) {
      Serial.print(main);
    }
  }

  void Debug(float main, bool bswitch) {
    if (bswitch) {
      Serial.print(main);
    }
  }

  void Debug(String main, bool bswitch) {
    if (bswitch) {
      Serial.print(main);
    }
  }

  void Debugln(const char* main, bool bswitch) {
    if (bswitch) {
      Serial.println(main);
    }
  }

  void Debugln(int main, bool bswitch) {
    if (bswitch) {
      Serial.println(main);
    }
  }

  void Debugln(float main, bool bswitch) {
    if (bswitch) {
      Serial.println(main);
    }
  }

  void Debugln(String main, bool bswitch) {
    if (bswitch) {
      Serial.println(main);
    }
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
////////////////////////////////////////////////////////////////////////////////

