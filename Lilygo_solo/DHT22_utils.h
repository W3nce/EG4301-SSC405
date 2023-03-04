#include "DHT.h"
#include "Wire.h"

#define XIAOESP32C3

// #define WROOMESP32DEV4
// #define LILYGO7600WROVER

#ifdef XIAOESP32C3
#define DHTPIN D10; //XIAO  

#else
#ifdef WROOMESP32DEV4
#define DHTPIN 12 //ESP32 Wroom Dev

#else 
#ifdef LILYGO7600WROVER
#define DHTPIN 12 //ESP32 Wroom Dev

#endif

#define DHTTYPE DHT22  // DHT 22  (AM2302), AM2321 // DHT 11 // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

#ifndef UART_BAUD
#define UART_BAUD           115200
#endif

//RX_TX Configuration
#ifndef BOARD_TX
#define BOARD_TX            D6
#endif

#ifndef BOARD_RX
#define BOARD_RX            D7
#endif

static float DHT22_Temperature;
static float DHT22_Humidity;

DHT dht(DHTPIN, DHTTYPE);

void DHT22_Start(){
  Serial1.println(F("DHT22 test"));
  dht.begin();  
  Serial1.println();
  
}



void getDHTValues(){
  dht.read();
  DHT22_Temperature = dht.readTemperature();
  DHT22_Humidity = dht.readHumidity();

  Serial1.print("Temperature = ");
  Serial1.print(DHT22_Temperature, 1);
  Serial1.println(" *C");
  
  Serial1.print("\t Humidity = ");
  Serial1.print(DHT22_Humidity, 1);
  Serial1.println(" %");
  Serial1.println();
  
}

/*
void setup()
{
  Serial1.begin(UART_BAUD, SERIAL_8N1, BOARD_RX, BOARD_TX);
  Serial.begin(UART_BAUD);
  Wire.begin();
  Wire.setClock(100000);
  DHT22_Start()
}

void loop()
{
  getDHTValues();
  delay(5000);
}
*/