/*
  SHT30 Temperature & Humidity Sensor
  modified on 13 Oct 2020
  by Mohammad Reza Akbari @ Electropeak
  Home

  Based on Library Example
*/
#include "Wire.h"
#include <SHT31.h>

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

uint32_t start;
uint32_t stop;

static float SHT3X_Temperature;
static float SHT3X_Humidity;

SHT31 sht;

void SHT3X_Start(){
  Serial1.println(F("SHT3X test"));
  sht.begin(0x44);    //Sensor I2C Address
  uint16_t stat = sht.readStatus();
  Serial1.print(stat, HEX);
  Serial1.println();
  
}

void getSHTValues(){
  sht.read();
  SHT3X_Temperature = sht.getTemperature();
  SHT3X_Humidity = sht.getHumidity();

  Serial1.print("Temperature = ");
  Serial1.print(SHT3X_Temperature, 1);
  Serial1.println(" *C");
  
  Serial1.print("\t Humidity = ");
  Serial1.print(SHT3X_Humidity, 1);
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
  SHT3X_Start()
}

void loop()
{
  getSHTValues();
  delay(5000);
}
*/