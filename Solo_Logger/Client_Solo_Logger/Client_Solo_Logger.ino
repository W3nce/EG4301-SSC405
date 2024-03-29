/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <cstring>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Libraries to get time from NTP Server
#include <NTPClient.h>
#include <WiFiUdp.h>

// Replace with your network credentials
/*const char* ssid     = "NOKIA-7480";
const char* password = "gyL9AJUiLd";*/

const char* ssid = "SINGTEL-A458";
const char* password = "quoophuuth";

/*const char* ssid = "Terence";
const char* password = "qwertyios";*/

#define LEAP_YEAR(Y) ((Y > 0) && !(Y % 4) && ((Y % 100) || !(Y % 400)))

#define temperatureCelsius

//Device name
String name = "Sussy";

// Timer variables
unsigned long lastTime = 0;
//Logging Checking Interval
unsigned long loggingInterval = 3000;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

//BME Sensor
Adafruit_BME280 bme;  // I2C

// Show Readings
int show = 0;

void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
}

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


//function that prints the latest sensor readings in the OLED display
void sendReadings(String tempData, String humData, String DateTimeData) {

  WiFiClientSecure* client = new WiFiClientSecure;
  //String hostserver = "https://192.168.18.6:6802/postEnv"; local server
  //String hostserver = "https://morning-peak-35514.herokuapp.com/postEnv";  //web server
  String hostserver = "https://sussylogger.fly.dev/postEnv";  //web server
  if (client) {
    //    client -> setCACert(rootCACertificate);
    client->setInsecure();  //comment to test secure connection
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

            String httpRequestData = "childid=" + name + "&time=" + DateTimeData + "&temperature=" + tempData + "&humidity=" + humData;
            Serial.print("Post Request Data String: ");
            Serial.println(httpRequestData);
            https.addHeader("Content-Type", "application/x-www-form-urlencoded");
            int httpResponseCode = https.POST(httpRequestData);

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

WiFiMulti WiFiMulti;

void setup() {
  //Start serial communication
  Serial.begin(115200);
  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  //pinMode(ledPin1, OUTPUT);
  //digitalWrite(ledPin1, HIGH);

  Serial.println("Starting Arduino BLE Client application...");
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);

  //WiFiMulti.addAP(ssid, password);  // Change to reflect hotspot/home network

  // wait for WiFi connection
  Serial.printf("Connecting to %s", ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  /*while ((WiFiMulti.run() != WL_CONNECTED)) {
    Serial.print(".");
  }*/
  Serial.println(" connected");

  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  // Init BME Sensor
  initBME();
}

void loop() {
  int waittime = 5;
  if ((millis() - lastTime) > loggingInterval) {
    Serial.print("Start Reading @ ");
    String DateTimeData = getFormattedDate();
    Serial.println(DateTimeData);

    float temp = bme.readTemperature();
    float tempF = 1.8 * temp + 32;
    float hum = bme.readHumidity();

//Notify temperature reading from BME sensor
#ifdef temperatureCelsius
    static char temperatureCTemp[6];
    dtostrf(temp, 6, 2, temperatureCTemp);
    Serial.print("Temperature Celsius: ");
    Serial.print(temperatureCTemp);
    Serial.print(" deg C");
#else
    static char temperatureFTemp[6];
    dtostrf(tempF, 6, 2, temperatureFTemp);
    Serial.print("Temperature Fahrenheit: ");
    Serial.print(temperatureFTemp);
    Serial.print(" ºF");
#endif

    //Notify humidity reading from BME
    static char humidityTemp[6];
    dtostrf(hum, 6, 2, humidityTemp);
    Serial.print(" - Humidity: ");
    Serial.print(humidityTemp);
    Serial.println(" %");

    Serial.printf("%f , %f \n", temp, hum);
    static char ptemp[7];
    static char phum[7];

    dtostrf(temp, 6, 2, ptemp);
    dtostrf(hum, 6, 2, phum);

    String tempData = &ptemp[1];

    String humData = &phum[1];

    sendReadings(tempData,humData,DateTimeData);


    lastTime = millis();
  }
  //delay(3000);  // Delay a second between loops.
}