#include <cstring>

// Libraries to connect to WiFi and send Readings to the Cloud
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

// Libraries for the BME280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Libraries to get time from NTP Server
#include <NTPClient.h>
#include <WiFiUdp.h>

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

// Replace with your network credentials
//const char *ssid = "YOUR_WIFI_SSID";
//const char *password = "YOUR_WIFI_PASSWORD";

// Replace with your network credentials
/*const char* ssid     = "NOKIA-7480";
const char* password = "gyL9AJUiLd";*/

const char *ssid = "SINGTEL-A458";
const char *password = "quoophuuth";

//const char* ssid = "Terence";
//const char* password = "qwertyios";

//To calculate leap year
#define LEAP_YEAR(Y) ((Y > 0) && !(Y % 4) && ((Y % 100) || !(Y % 400)))

//Logging Temperature in Celcius
#define temperatureCelsius

//Device name
String name = "YOUR_DEVICE_NAME";

//Deep Sleep Time/ logging frequency
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 600      /* 600 seconds */

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
//,arr4,arr5,arr6,arr7,arr8,arr9,arr10,arr11
RTC_DATA_ATTR static char *TimeArray[ReadingsThreshold] = { arr0, arr1, arr2, arr3 };

// counts the total number of readings taken for indexing
RTC_DATA_ATTR static int datacount = 0;  

//Initially false, after micro-SD Card initialised and cleared for use, becomes true
RTC_DATA_ATTR static bool SDInit = false;

//Allocate Space for WifiClient and HTTPClient
static WiFiClientSecure *client;
static HTTPClient https;

// Allocate and create NTP Client named timeCLient
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//Allocate Space for BME Sensor
Adafruit_BME280 bme;  // I2C

// ledPin refers to ESP32 GPIO 17
const int ledPin = 17;

//Allocated Space to calculate program run time
unsigned long startTime;

//Allocate to save Readings as a String to append to CSV
String dataString;


//Starts BME280 Sensor
void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
}
//Returns a Formatted Date a the form YYYY-MM-DDTHH:MM:SSZ
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
  // We need to extract date and time
  String formattedDate = getFormattedDate();
  //Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  String dayStamp = formattedDate.substring(0, splitT);
  //Serial.println(dayStamp);
  // Extract time
  String timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  //Serial.println(timeStamp);
}
//Print Wake Up Reason due to Timer
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
//Creates and Starts the HTTPS Client
bool createClient() {
  client = new WiFiClientSecure;
  //String hostserver = "https://192.168.18.6:6802/postEnv"; local server
  //String hostserver = "https://morning-peak-35514.herokuapp.com/postEnv";  //web server
  String hostserver = "https://sussylogger.fly.dev/postEnv";  //web server

  if (client) {
    //    client -> setCACert(rootCACertificate);
    client->setInsecure();  //comment to test secure connection
    {                       // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
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
            return true;
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
          return true;
        }
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }

      // End extra scoping block
    }
  } else {
    Serial.println("Unable to create client");
  }
  return false;
}
//Deletes the HTTPS Client
void deleteClient() {
  delete client;
}

//Sends a reading through HTTPS
void sendReading(String tempData, String humData, String DateTimeData) {
  String tableid = String("testing");
  String httpRequestData = "tableid=" + tableid + "&childid=" + name + "&time=" + DateTimeData + "&temperature=" + tempData + "&humidity=" + humData;
  Serial.print("Post Request Data String: ");
  Serial.println(httpRequestData);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpResponseCode = https.POST(httpRequestData);

  Serial.print("HTTP POST Response code: ");
  Serial.println(httpResponseCode);
  
  delay(500);
}
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
//Connect to the WIFI using the given SSID and PASSWORD
void connectWIFI() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
}
/*

  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());*/

void setup() {
  //Saves the current time in milliSeconds
  startTime = millis();

  Serial.println("Starting Arduino BLE Client application...");
  
  //Initialise and Turn on LED
  pinMode(ledPin, OUTPUT); 
  digitalWrite(ledPin, HIGH);

  //Start serial communication
  Serial.begin(115200);

  //Prints WakeUp Reason
  print_wakeup_reason();

  //Connect to the WIFI using the given SSID and PASSWORD
  connectWIFI();

  //Intialise NTP Client and Update the current time over internet
  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  // Initialise BME Sensor
  initBME();
  Serial.print("BME Initialised, Start Reading @ ");

  //Get a Formatted Date a the form YYYY-MM-DDTHH:MM:SSZ 
  //and save in TimeArray (Deep Sleep Memory)
  String DateTimeData = getFormattedDate();
  char *temparr = TimeArray[ReadingsNotSent];
  strcpy(temparr, DateTimeData.c_str());
  Serial.print("Date OK ");

  //Allocate space, Read and Save Temperature in Deep Sleep Memory
  float temp = bme.readTemperature();
  float tempF = 1.8 * temp + 32;
  float hum = bme.readHumidity();
  Serial.print("Temp and Hum OK ");
  TempArray[ReadingsNotSent] = temp;
  HumArray[ReadingsNotSent] = hum;

  //Shows how many Readings are stored in Deep Sleep Memory
  Serial.println(TimeArray[ReadingsNotSent]);
  Serial.println("Values Stored in RTC");
  
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
    writeFile(SD, "/data.csv", "Index,Temperature,Humidty,Date \r\n");
  } else {
    Serial.println("File already exists");
    if(!SDInit){
    //delete and recreate?
    deleteFile(SD, "/data.csv");
    writeFile(SD, "/data.csv", "Index,Temperature,Humidty,Date \r\n");
    Serial.println("File Recreated");
    SDInit = true;
    }
  }
  file.close();  
  Serial.println("SD Card Initialised");

  //Increase Index by 1 and creates the line of Readings to be appended
  datacount++;
  dataString = String(datacount) + "," + String(temp) + "," + String(hum) + "," + DateTimeData;
  Serial.println(dataString);
  if (writeDataLine(SD, "/data.csv", dataString.c_str())) {
    Serial.println("New Data added");
    readFile(SD, "/data.csv");
  } else {
    Serial.println("New Data not added");
  }
  //Shows Number of Readings Saved in Deep Sleep Memory and not sent to Cloud
  ++ReadingsNotSent;
  Serial.println("Readings Not Sent: " + String(ReadingsNotSent) + "/" + String(ReadingsThreshold));

  //Start HTTPClient to Send Readings to cloud if ReadingsThreshold is Reached
  if (ReadingsNotSent >= ReadingsThreshold && createClient()) {
    for (int i = 0; i < ReadingsThreshold; i++) {
      char ptemp[7];
      char phum[7];

      dtostrf(TempArray[i], 6, 2, ptemp);
      dtostrf(HumArray[i], 6, 2, phum);
      String tempData = &ptemp[1];
      String humData = &phum[1];
      Serial.println(TimeArray[i]);
      String timeData = TimeArray[i];
      sendReading(tempData, humData, timeData);
    }
    deleteClient();
    ReadingsNotSent = 0;
  }

  //Start Deep Sleep for (TIME_TO_SLEEP - TIME_TAKEN_TO_RUN_PROGRAM)
  float TimeDiff = (float)TIME_TO_SLEEP - ((float)(millis() - startTime) / 1000) ;
  if (TimeDiff < 0) {
    Serial.println("Program took" + String(-TimeDiff)+
    "Seconds more than Sleep Time ("+String(TIME_TO_SLEEP)+"s)");    
    esp_sleep_enable_timer_wakeup(0);

  } else {
    int SleepTimeMS = TimeDiff * uS_TO_S_FACTOR;
    esp_sleep_enable_timer_wakeup(SleepTimeMS);

  }
  Serial.println("Setup ESP32 to sleep for every " + String((TimeDiff<0?0:TimeDiff)) + " Seconds");

  Serial.println("Going to sleep now");
  Serial.flush();
  Serial.print(String(millis() - startTime).c_str());
  Serial.println(" MilliSeconds Taken");

  digitalWrite(ledPin, LOW);
  esp_deep_sleep_start();
}

void loop() {
}