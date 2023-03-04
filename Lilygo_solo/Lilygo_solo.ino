#define TINY_GSM_MODEM_SIM7600

// Header File for TinyGSM and LED and WIFI and Function prototypes
#include <Ticker.h>
#include <TinyGsmClient.h>
#include "utilities.h"
#include "Lilygo_solo.h"

#include <cstring>

// Libraries to connect to WiFi and send Readings to the Cloud
#include <ArduinoHttpClient.h>

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

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

//Number of readings to store before sending in RTC Recovery Memory
#define ReadingsThreshold 4
//Number of readings currently stored in RTC Recovery Memory
RTC_DATA_ATTR static int ReadingsNotSent = 0;

//Allocating Space in the RTC/Deep Sleep Memory for Temperature and Humidity
RTC_DATA_ATTR static float TempArray[ReadingsThreshold];
RTC_DATA_ATTR static float HumArray[ReadingsThreshold];

//Allocating Space in the RTC/Deep Sleep Memory for Date and Time String
RTC_DATA_ATTR static char arr0[20 + 1];
RTC_DATA_ATTR static char arr1[20 + 1];
RTC_DATA_ATTR static char arr2[20 + 1];
RTC_DATA_ATTR static char arr3[20 + 1];
/*
RTC_DATA_ATTR static char arr4[20 + 1]; 
RTC_DATA_ATTR static char arr5[20 + 1]; 
RTC_DATA_ATTR static char arr6[20 + 1]; 
RTC_DATA_ATTR static char arr7[20 + 1]; 
RTC_DATA_ATTR static char arr8[20 + 1]; 
RTC_DATA_ATTR static char arr9[20 + 1]; 
RTC_DATA_ATTR static char arr10[20 + 1]; 
RTC_DATA_ATTR static char arr11[20 + 1]; 
*/
//,arr1, arr2, arr3,arr4,arr5,arr6,arr7,arr8,arr9,arr10,arr11
RTC_DATA_ATTR static char *TimeArray[ReadingsThreshold] = { arr0, arr1, arr2, arr3 };

// counts the total number of readings taken for indexing
RTC_DATA_ATTR static int datacount = 0;

//Initially false, after micro-SD Card initialised and cleared for use, becomes true
RTC_DATA_ATTR static bool SDInit = false;


//Allocate Space for BME Sensor
// Adafruit_BME280 bme;  // I2C


//Allocated Space to calculate program run time
unsigned long startTime;

//Allocate to save Readings as a String to append to CSV
String dataString;

//Allocate for storing current date and timec bad location
static String currDateTime;
static String gps_raw;
static float CurrLat;
static float CurrLong;

void setup() {

  /************************************************************************************
    Modem Related Setup
  ************************************************************************************/
  // Set console baud rate and Start serial communication
  SerialMon.begin(115200);
  Serial.begin(115200);
  delay(100);
  
  Serial.println("Starting Arduino BLE Client application...");

  //Saves the current time in milliSeconds
  startTime = millis();


  //Initialise and Turn on LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Set GSM module baud rate
  SerialAT.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);

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

  //Prints WakeUp Reason
  print_wakeup_reason();

  // Initialise Sensor
  // initBME();
  Sensor_Start();
  Serial.print("Sensor Initialised, Start Reading @ ");

  getValues();
  Serial.print("Temp and Hum OK ");
  TempArray[ReadingsNotSent] = temp;
  HumArray[ReadingsNotSent] = hum;

  //Shows how many Readings are stored in Deep Sleep Memory
  Serial.println(TimeArray[ReadingsNotSent]);
  Serial.println("Values Stored in RTC");


  //Test TinyGSM Module  
  TestWorkflow();
  //Arduino_TinyGSM_Test();
  
  // Method 1: Power off module by pulling the PWRKEY pin down to ground.
  // Method 2: Power off module by AT command “AT+CPOF”

  Serial.println("Off using CMD");
  sendData("AT+CPOF", 3000, false); 
  Serial.println("OFf using Pin");
  digitalWrite(MODEM_PWRKEY, LOW);

  //Get a Formatted Date a the form YYYY-MM-DDTHH:MM:SSZ
  //and save in TimeArray (Deep Sleep Memory)
  String DateTimeData = currDateTime;
  // getFormattedDate();

  char *temparr = TimeArray[ReadingsNotSent];
  strcpy(temparr, DateTimeData.c_str());
  Serial.print("Date OK ");

  //Initialises SD by clearing SD card if needed
  SD_init();

  //Increase Index by 1 and creates the line of Readings to be appended
  datacount++;
  dataString = String(datacount) + "," + String(temp) + "," + String(hum) + "," + DateTimeData + "," + String(CurrLat) + "," + String(CurrLong);
  Serial.println(dataString);
  if (SDInit && writeDataLine(SD, "/data.csv", dataString.c_str())) {
    Serial.println("New Data added");
    readFile(SD, "/data.csv");
  } else {
    Serial.println("New Data not added");
  }

  //Shows Number of Readings Saved in Deep Sleep Memory and not sent to Cloud
  ++ReadingsNotSent;
  Serial.println("Readings Not Sent: " + String(ReadingsNotSent) + "/" + String(ReadingsThreshold));

  //Start HTTPClient to Send Readings to cloud if ReadingsThreshold is Reached
  if (ReadingsNotSent >= ReadingsThreshold) {  //&& createClient()
    for (int i = 0; i < ReadingsThreshold; i++) {
      char ptemp[7];
      char phum[7];

      dtostrf(TempArray[i], 6, 2, ptemp);
      dtostrf(HumArray[i], 6, 2, phum);
      String tempData = &ptemp[1];
      String humData = &phum[1];
      Serial.println(TimeArray[i]);
      String timeData = TimeArray[i];
      //sendReading(tempData, humData, timeData);
    }
    //deleteClient();
    ReadingsNotSent = 0;
  }

  //Start Deep Sleep for (TIME_TO_SLEEP - TIME_TAKEN_TO_RUN_PROGRAM)
  float TimeDiff = (float)TIME_TO_SLEEP - ((float)(millis() - startTime) / 1000);

  if (TimeDiff < 0) {
    Serial.println("Program took" + String(-TimeDiff) + "Seconds more than Sleep Time (" + String(TIME_TO_SLEEP) + "s)");
    esp_sleep_enable_timer_wakeup(0);

  } else {
    int SleepTimeMS = TimeDiff * uS_TO_S_FACTOR;
    esp_sleep_enable_timer_wakeup(SleepTimeMS);
  }

  Serial.println("Setup ESP32 to sleep for every " + String((TimeDiff < 0 ? 0 : TimeDiff)) + " Seconds");

  Serial.println("Going to sleep now");
  //Serial.flush();
  Serial.print(String(millis() - startTime).c_str());
  Serial.println(" MilliSeconds Taken\n");

  digitalWrite(LED_PIN, LOW);  
  Serial.println("Sleep");
  esp_deep_sleep_start();
}

void loop() {
}

void light_sleep(uint32_t sec) {
  esp_sleep_enable_timer_wakeup(sec * 1000000ULL);
  esp_light_sleep_start();
}

//Starts BME280 Sensor
// void initBME() {
//   if (!bme.begin(0x76)) {
//     Serial.println("Could not find a valid BME280 sensor, check wiring!");
//     while (1)
//       ;
//   }
// }

//Initialises SD Card
void SD_init() {
  // Initialise micro-SD Card Module
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/data.csv");
  if (!file) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/data.csv", "Index,Temperature,Humidty,DateTime,Lat,Long\r\n");
  } else {
    Serial.println("File already exists");
    if (!SDInit) {
      //delete and recreate?
      deleteFile(SD, "/data.csv");
      writeFile(SD, "/data.csv", "Index,Temperature,Humidty,DateTime,Lat,Long \r\n");
      Serial.println("File Recreated");
      SDInit = true;
    }
  }
  file.close();
  Serial.println("SD Card Initialised");
}
//Returns a Formatted Date a the form YYYY-MM-DDTHH:MM:SSZ
String getFormattedDate() {
  return currDateTime;
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

//Sends a reading through HTTPS
// void sendReading(String tempData, String humData, String DateTimeData) {
//   String tableid = String("demotwo");
//   String httpRequestData = "tableid=" + tableid + "&childid=" + DEVICE_NAME + "&time=" + DateTimeData + "&temperature=" + tempData + "&humidity=" + humData;
//   Serial.print("Post Request Data String: ");
//   Serial.println(httpRequestData);
//   https.addHeader("Content-Type", "application/x-www-form-urlencoded");
//   int httpResponseCode = https.POST(httpRequestData);

//   Serial.print("HTTP POST Response code: ");
//   Serial.println(httpResponseCode);
  
//   delay(500);
// }

//File Management Commands

//Show all Files/Folder in given directory and the Files in Each Files based on depth given
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.print(file.name());
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.print(file.size());
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    }
    file = root.openNextFile();
  }
}
//Creates Folder in given directory
bool createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
    return true;
  } else {
    Serial.println("mkdir failed");
    return false;
  }
}
//Delete Folder in given directory
bool removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
    return true;
  } else {
    Serial.println("rmdir failed");
    return false;
  }
}
//Reads a File in given directory
bool readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return false;
  }

  Serial.print("Read from file: \n");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
  return true;
}
//Write/Rewrite a File in given directory
bool writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  if (file.print(message)) {
    Serial.println("File written");
    return true;
  } else {
    Serial.println("Write failed");
    return false;
  }
  file.close();
}
//Appends another line in a File in given directory
bool appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return false;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
    return true;
  } else {
    Serial.println("Append failed");
    return false;
  }
  file.close();
}
//Rename a File in given directory
bool renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
    return true;
  } else {
    Serial.println("Rename failed");
    return false;
  }
}
//Deletes a File in given directory
bool deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
    return true;
  } else {
    Serial.println("Delete failed");
    return false;
  }
}

//Append a line of Reading in the CSV in the given directory
bool writeDataLine(fs::FS &fs, const char *path, const char *messageLine) {

  Serial.printf("Writing file: %s\n", path);

  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending line");
    return false;
  }
  if (file.println(messageLine)) {
    Serial.println("Line Appended");
    return true;
  } else {
    Serial.println("Line Append failed");
    return false;
  }
  file.close();
}

void TestWorkflow() {
  Serial.println("Start Modem");
  StartModem();

  Serial.println("Start GPRS");
  StartGPRS();

  /*
  Serial.println("Start GPS session");
  sendData("AT+CGPS=1", 3000, DEBUG_MODE);  // Start GPS session 

  Serial.println("Get GPS fixed position information");
  sendData("AT+CGPSINFO", 3000, DEBUG_MODE); // Get GPS fixed position information

  Serial.println("Stop GPS session");
  sendData("AT+CGPS=0", 3000, DEBUG_MODE);  // Stop GPS session
  */

  Serial.println("Get GPS Func");
  GetGPS();

  Serial.println("Get Time Func");
    GetTime_TinyGSM();

  // Serial.println("Get URL Func");
  Serial.println("TinyGSM Client google GET");
  SendGetHttpRequest_TinyGSM();
  Serial.println("TinyGSM Client POST to Sussy");
  SendPostHttpRequest_TinyGSM();

  Serial.println("Stop Modem");
  StopModem();
}

void StartModem() {
  Serial.println("Initializing modem...");
  if (!modem.init()) {
    Serial.println("Failed to restart modem, delaying 10s and retrying");
    return;
  }
}

void StopModem() {
  modem.gprsDisconnect();
  light_sleep(5);
  if (!modem.isGprsConnected()) {
    Serial.println("GPRS disconnected");
  } else {
    Serial.println("GPRS disconnect: Failed.");
  }
  modem.poweroff();
}

void StartGPRS() {
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
  DBG("setNetworkMode:", ret);


  //https://github.com/vshymanskyy/TinyGSM/pull/405
  uint8_t mode = modem.getGNSSMode();
  DBG("GNSS Mode:", mode);

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
  light_sleep(1);

  String name = modem.getModemName();
  DBG("Modem Name:", name);

  String modemInfo = modem.getModemInfo();
  DBG("Modem Info:", modemInfo);

  // Unlock your SIM card with a PIN if needed
  if (GSM_PIN && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }

  Serial.println("Waiting for network...");
  if (!modem.waitForNetwork(600000L)) {
    light_sleep(10);
    return;
  }

  if (modem.isNetworkConnected()) {
    Serial.println("Network connected");
  }

  // Connect to APN
  Serial.print("Connecting to APN: ");
  Serial.println(APN);
  DBG("Connecting to", APN);
  if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
    light_sleep(10);
    return;
  }

  //res will represent the modem connection's success/ failure
  bool res = modem.isGprsConnected();
  Serial.println("GPRS status:" + res ? "connected" : "not connected");

  String ccid = modem.getSimCCID();
  DBG("CCID:", ccid);

  String imei = modem.getIMEI();
  DBG("IMEI:", imei);

  String imsi = modem.getIMSI();
  DBG("IMSI:", imsi);

  String cop = modem.getOperator();
  DBG("Operator:", cop);

  IPAddress local = modem.localIP();
  DBG("Local IP:", local);

  int csq = modem.getSignalQuality();
  DBG("Signal quality:", csq);
}

void SendPostHttpRequest_TinyGSM() {

  const char server[] = "sussylogger-2.fly.dev";  //
  const int port = 80;

  int shipmentID = 4;
  String nS = "N";
  String eW = "E";

  String contentType = "application/x-www-form-urlencoded";
  String postData =   "shipmentID=" + String(shipmentID) 
                    // + "&childid=" + childName 
                    + "&temperature=" + String(temp) 
                    + "&humidity=" + String(hum) 
                    + "&latitude=" + String(CurrLat )
                    + "&nS=" + nS 
                    + "&longitude=" + String(CurrLong) 
                    + "&eW=" + eW
                    + "&DateTime=" + currDateTime;

  TinyGsmClient client(modem);
  // SSLClient secure_presentation_layer(&client);
  // secure_presentation_layer.setCACert(root_ca);
  HttpClient http = HttpClient(client, server, port);
  // HttpClient http(client, server, port);
  SerialMon.print(F("Performing HTTP POST request to ... "));
  SerialMon.print(F(server));
  SerialMon.println("");
  SerialMon.println(postData);

  // http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  http.beginRequest();
  int err = http.post("/api/master/updateShipment");
  Serial.println("Error Code: " + String(err));
  http.sendHeader("Content-Type", "application/x-www-form-urlencoded");
  http.sendHeader("Content-Length", postData.length());
  // http.sendHeader("X-Custom-Header", "custom-header-value");
  http.beginBody();
  http.print(postData);
  http.endRequest();
  // http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  // http.setHttpResponseTimeout(120);
  // http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);


  // I got the 302 code but with HTTPC_STRICT_FOLLOW_REDIRECTS or HTTPC_FORCE_FOLLOW_REDIRECTS

  // int err = 
  // http.post("/api/master/updateShipment/", contentType, postData);
  // if (err != 0) {
  //   SerialMon.println(F(" failed to connect"));
  //   Serial.println("Retrying...");
  //   // http.post("/api/master/updateShipment/", contentType, postData);
  //   delay(10000);
  //   // return;
  // }
  SerialMon.println(" ");

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(10000);
    return;
  }

  SerialMon.println(F("Response Headers:"));
  while (http.headerAvailable()) {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());

  // Shutdown

  http.stop();
  SerialMon.println(F("Server disconnected"));
}

void SendGetHttpRequest_TinyGSM() {

  const char server[] = "sussylogger-2.fly.dev";  //
  const char resource[] = "/";
  const int port = 80;

  TinyGsmClient client(modem);
  HttpClient http(client, server, port);
  SerialMon.print(F("Performing HTTP GET request to ... "));
  SerialMon.print(F(server));
  // http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  int err = http.get(resource);
  if (err != 0) {
    SerialMon.println(F(" failed to connect"));
    delay(10000);
    // return;
  }
  SerialMon.println(" ");

  int status = http.responseStatusCode();
  SerialMon.print(F("Response status code: "));
  SerialMon.println(status);
  if (!status) {
    delay(10000);
    return;
  }

  SerialMon.println(F("Response Headers:"));
  while (http.headerAvailable()) {
    String headerName = http.readHeaderName();
    String headerValue = http.readHeaderValue();
    SerialMon.println("    " + headerName + " : " + headerValue);
  }

  int length = http.contentLength();
  if (length >= 0) {
    SerialMon.print(F("Content length is: "));
    SerialMon.println(length);
  }
  if (http.isResponseChunked()) {
    SerialMon.println(F("The response is chunked"));
  }

  String body = http.responseBody();
  SerialMon.println(F("Response:"));
  SerialMon.println(body);

  SerialMon.print(F("Body length is: "));
  SerialMon.println(body.length());

  // Shutdown

  http.stop();
  SerialMon.println(F("Server disconnected"));
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
      DBG("Couldn't get network time, retrying in 15s.");
      light_sleep(15);
    }
  }
  DBG("Retrieving time again as a string");
  String datefull = modem.getGSMDateTime(DATE_FULL);
  String datetime = modem.getGSMDateTime(DATE_TIME);
  String datedate = modem.getGSMDateTime(DATE_DATE);
  //23/03/03,00:39:54+32
  //2023-02-28T19:20:01
  currDateTime = String(year3) + "-" + datefull.substring(3,5) + "-" + datefull.substring(6,8) + "T" + datetime.substring(0,2) + ":" + datetime.substring(3,5) + ":" + datetime.substring(6,8);
  currDateTime = currDateTime.c_str();
  Serial.println("Current Network Date_Full: " + datefull) ;
  Serial.println("Current Network Date_Time: " + datetime);
  Serial.println("Current Network Date_Date: " + datedate);
  Serial.println("Current Network My Time: " + currDateTime);
}

void GetTCP(const char *URL) {
  TinyGsmClient client(modem, 0);
  const int port = 80;
  DBG("Connecting to ", URL);
  if (!client.connect(URL, port)) {
    DBG("... failed");
  } else {
    // Make a HTTP GET request:
    client.print(String("GET ") + "Target" + " HTTP/1.0\r\n");
    client.print(String("Host: ") + URL + "\r\n");
    client.print("Connection: close\r\n\r\n");

    // Wait for data to arrive
    uint32_t start = millis();
    while (client.connected() && !client.available() && millis() - start < 30000L) {
      delay(100);
    };

    // Read data
    start = millis();
    while (client.connected() && millis() - start < 5000L) {
      while (client.available()) {
        SerialMon.write(client.read());
        start = millis();
      }
    }
    client.stop();
  }
}

void GetGPS() {
  DBG("Enabling GPS/GNSS/GLONASS");
  Serial.println("Enabling GPS/GNSS/GLONASS");
  modem.enableGPS();
  light_sleep(2);

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
  gps_raw = modem.getGPSraw();
  
  DBG("GPS/GNSS Based Location String:", gps_raw);
  Serial.print("GPS/GNSS Based Location String: ");
  Serial.println(gps_raw);
  DBG("Disabling GPS");
  Serial.println("Disabling GPS");
  modem.disableGPS();
}

void Arduino_TinyGSM_Test() {
  //res will represent the modem connection's success/ failure
  bool res;

  // Restart takes quite some time
  // To skip it, call init() instead of restart()

  DBG("Initializing modem...");
  if (!modem.init()) {
    DBG("Failed to restart modem, delaying 10s and retrying");
    return;
  }

  // if (!modem.restart()) {
  //     DBG("Failed to restart modem, delaying 10s and retrying");
  //     // restart autobaud in case GSM just rebooted
  //     return;
  // }

  #if TINY_GSM_TEST_GPRS
    /*  Preferred mode selection : AT+CNMP
            2 – Automatic
            13 – GSM Only
            14 – WCDMA Only
            38 – LTE Only
            59 – TDS-CDMA Only
            9 – CDMA Only
            10 – EVDO Only
            19 – GSM+WCDMA Only
            22 – CDMA+EVDO Only
            48 – Any but LTE
            60 – GSM+TDSCDMA Only
            63 – GSM+WCDMA+TDSCDMA Only
            67 – CDMA+EVDO+GSM+WCDMA+TDSCDMA Only
            39 – GSM+WCDMA+LTE Only
            51 – GSM+LTE Only
            54 – WCDMA+LTE Only
      */
    String ret;
    //    do {
    //        ret = modem.setNetworkMode(2);
    //        delay(500);
    //    } while (ret != "OK");
    ret = modem.setNetworkMode(2);
    DBG("setNetworkMode:", ret);


    //https://github.com/vshymanskyy/TinyGSM/pull/405
    uint8_t mode = modem.getGNSSMode();
    DBG("GNSS Mode:", mode);

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
    light_sleep(1);

    String name = modem.getModemName();
    DBG("Modem Name:", name);

    String modemInfo = modem.getModemInfo();
    DBG("Modem Info:", modemInfo);

    // Unlock your SIM card with a PIN if needed
    if (GSM_PIN && modem.getSimStatus() != 3) {
      modem.simUnlock(GSM_PIN);
    }

    DBG("Waiting for network...");
    if (!modem.waitForNetwork(600000L)) {
      light_sleep(10);
      return;
    }

    if (modem.isNetworkConnected()) {
      DBG("Network connected");
    }
  #endif

  #if TINY_GSM_TEST_GPRS
    DBG("Connecting to", APN);
    if (!modem.gprsConnect(APN, GPRS_USER, GPRS_PASS)) {
      light_sleep(10);
      return;
    }

    res = modem.isGprsConnected();
    DBG("GPRS status:", res ? "connected" : "not connected");

    String ccid = modem.getSimCCID();
    DBG("CCID:", ccid);

    String imei = modem.getIMEI();
    DBG("IMEI:", imei);

    String imsi = modem.getIMSI();
    DBG("IMSI:", imsi);

    String cop = modem.getOperator();
    DBG("Operator:", cop);

    IPAddress local = modem.localIP();
    DBG("Local IP:", local);

    int csq = modem.getSignalQuality();
    DBG("Signal quality:", csq);
  #endif

  #if TINY_GSM_TEST_USSD && defined TINY_GSM_MODEM_HAS_SMS
    String ussd_balance = modem.sendUSSD("*111#");
    DBG("Balance (USSD):", ussd_balance);

    String ussd_phone_num = modem.sendUSSD("*161#");
    DBG("Phone number (USSD):", ussd_phone_num);
  #endif

  #if TINY_GSM_TEST_TCP && defined TINY_GSM_MODEM_HAS_TCP
    TinyGsmClient client(modem, 0);
    const int port = 80;
    DBG("Connecting to ", TARGET_SERVER);
    if (!client.connect(TARGET_SERVER, port)) {
      DBG("... failed");
    } else {
      // Make a HTTP GET request:
      client.print(String("GET ") + TARGET_RESOURCE + " HTTP/1.0\r\n");
      client.print(String("Host: ") + TARGET_SERVER + "\r\n");
      client.print("Connection: close\r\n\r\n");

      // Wait for data to arrive
      uint32_t start = millis();
      while (client.connected() && !client.available() && millis() - start < 30000L) {
        delay(100);
      };

      // Read data
      start = millis();
      while (client.connected() && millis() - start < 5000L) {
        while (client.available()) {
          SerialMon.write(client.read());
          start = millis();
        }
      }
      client.stop();
    }
  #endif

  #if TINY_GSM_TEST_CALL && defined(CALL_TARGET)

    DBG("Calling:", CALL_TARGET);
    SerialAT.println("ATD" CALL_TARGET ";");
    modem.waitResponse();
    light_sleep(20);
  #endif

  #if TINY_GSM_TEST_GPS && defined TINY_GSM_MODEM_HAS_GPS
    DBG("Enabling GPS/GNSS/GLONASS");
    modem.enableGPS();
    light_sleep(2);

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
    DBG("Requesting current GPS/GNSS/GLONASS location");
    for (;;) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      if (modem.getGPS(&lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                      &year2, &month2, &day2, &hour2, &min2, &sec2)) {
        DBG("Latitude:", String(lat2, 8), "\tLongitude:", String(lon2, 8));
        DBG("Speed:", speed2, "\tAltitude:", alt2);
        DBG("Visible Satellites:", vsat2, "\tUsed Satellites:", usat2);
        DBG("Accuracy:", accuracy2);
        DBG("Year:", year2, "\tMonth:", month2, "\tDay:", day2);
        DBG("Hour:", hour2, "\tMinute:", min2, "\tSecond:", sec2);
        break;
      } else {
        light_sleep(2);
      }
    }
    DBG("Retrieving GPS/GNSS/GLONASS location again as a string");
    String gps_raw = modem.getGPSraw();
    DBG("GPS/GNSS Based Location String:", gps_raw);
    DBG("Disabling GPS");
    modem.disableGPS();
  #endif

  #if TINY_GSM_TEST_TIME && defined TINY_GSM_MODEM_HAS_TIME
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
        DBG("Couldn't get network time, retrying in 15s.");
        light_sleep(15);
      }
    }
    DBG("Retrieving time again as a string");
    String time = modem.getGSMDateTime(DATE_FULL);
    DBG("Current Network Time:", time);
  #endif

  #if TINY_GSM_TEST_GPRS
    modem.gprsDisconnect();
    light_sleep(5);
    if (!modem.isGprsConnected()) {
      DBG("GPRS disconnected");
    } else {
      DBG("GPRS disconnect: Failed.");
    }
  #endif

  #if TINY_GSM_TEST_TEMPERATURE && defined TINY_GSM_MODEM_HAS_TEMPERATURE
    float temp = modem.getTemperature();
    DBG("Chip temperature:", temp);
  #endif

  #ifdef TEST_RING_RI_PIN

  #ifdef MODEM_RI
    //Set RI Pin input
    pinMode(MODEM_RI, INPUT);

    Serial.println("Wait for call in");
    //When is no calling ,RI pin is high level
    while (digitalRead(MODEM_RI)) {
      Serial.print('.');
      delay(500);
    }
    Serial.println("call in ");

    //Wait for 5 seconds to connect the call
    delay(5000);

    //Accept call
    SerialAT.println("ATA");

    // Hang up after 20 seconds of talk time
    delay(20000);

    SerialAT.println("ATH");

  #endif  //MODEM_RI

  #endif  //TEST_RING_RI_PIN

  #ifdef MODEM_DTR1

    modem.sleepEnable();

    delay(100);

    // test modem response , res == 0 , modem is sleep
    res = modem.testAT();
    Serial.print(" Test AT result -> ");
    Serial.println(res);

    delay(1000);

    Serial.println("Use DTR Pin Wakeup");
    pinMode(MODEM_DTR, OUTPUT);
    //Set DTR Pin low , wakeup modem .
    digitalWrite(MODEM_DTR, LOW);

    // test modem response , res == 1 , modem is wakeup
    res = modem.testAT();
    Serial.print(" Test AT result -> ");
    Serial.println(res);

  #endif

  #if TINY_GSM_POWERDOWN
    // Try to power-off (modem may decide to restart automatically)
    // To turn off modem completely, please use Reset/Enable pins
    modem.poweroff();
    DBG("Poweroff.");
  #endif

  SerialMon.println("End of tests.");
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