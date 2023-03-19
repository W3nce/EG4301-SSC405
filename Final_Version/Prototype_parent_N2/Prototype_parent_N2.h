#include "BLEDevice.h"
#include "BLEAdvertisedDevice.h"
#include <Wire.h>
#include <cstring>
#include <string>
#include "FS.h"
#include "SD.h"
#include "utilities.h"
#include <GeneralUtils.h>

// Settings Needed
static bool TurnOnScanResults = true;
static bool TurnOnNotiResults = true;
// static bool TurnOnConnResults = true;

//Default Temperature is in Celsius
#define temperatureCelsius

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

#define sizelimit_def 3
#define ServerAddressLength 17


static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

int strcicmp(char const *a, char const *b);
void connect_All();
bool connect_Server(BLEClient * client_test, BLEAdvertisedDevice * advdev_test);
bool in_ServerAddressArray(const char * TargetServerCharArray);

class ClientServerManager;
class ClientServer {
  friend class ClientServerManager;
  public: 
    ClientServer();

    //Address of the advertised Server found during scanning...
    BLEAddress *              pServerAddress;
    //Reference to the advertised Server found during scanning...
    BLEAdvertisedDevice *     pAdvertisedDevice;
    //Reference to the Connected Client after successful connection...
    BLEClient *               pClient;
    //Check if Init and Connected
    bool                      IsInit = false;
    bool                      IsConnected = false;
    // Check if Characteristics are Ready and Values are Updated
    struct ReadyCheckStruct { 
      bool CharReady = false;
      bool TempReady= false;
      bool HumReady = false;
    } ReadyCheck;

    struct NewDataStruct{ 
      std::string NewTemp;
      std::string NewHum;
    } NewData;

    void                      setDevice(BLEAdvertisedDevice *pAdvertisedDevice);
    // bool                      connectServer();

  
};

class ClientServerManager {
  public:
    ClientServerManager();

    const int                 sizelimit = sizelimit_def;
    ClientServer              Client_Test1;
    ClientServer              Client_Test2;
    ClientServer              Client_Test3;
    ClientServer *            MyClientServerList[sizelimit_def]; 
    int                       TotalClientServer = 0;
    int                       TotalConnectedClientServer = 0;
    float                     SelfTemp;
    float                     SelfHum;
    std::string               CurrDateTime;
    float                     SelfLat;
    float                     SelfLon;

    
    int                       getNumberofClientServer();
    bool                      addDevice(BLEAdvertisedDevice *pAdvDev);
    int                       contains(BLEAdvertisedDevice* AdvDev);
    bool                      checkAllConnected();
    bool                      checkAllReady();
    void                      setTempChar(std::string Server,std::string Value);
    void                      setHumChar(std::string Server,std::string Value);
    // bool                      checkReadings();
    void                      sendReadings(int &currdatacount);
    void                      storeAllReadings(int &currdatacount);
};
