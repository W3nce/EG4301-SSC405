/***************************************************************************
  This is a library for the BME280 humidity, temperature & pressure sensor

  Designed specifically to work with the Adafruit BME280 Breakout
  ----> http://www.adafruit.com/products/2650

  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface. The device's I2C address is either 0x76 or 0x77.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
  See the LICENSE file for details.
 ***************************************************************************/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#ifndef UART_BAUD
#define UART_BAUD           115200
#endif

//RX_TX Configuration
#ifndef BOARD_TX
#define BOARD_TX            1
#endif

#ifndef BOARD_RX
#define BOARD_RX            3
#endif


// #define BME_SCK 13
// #define BME_MISO 12
// #define BME_MOSI 11
// #define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

static float BME280_Temperature;
static float BME280_Humidity;
static float BME280_Pressure;
static float BME280_Altitude;

unsigned long delayTime = 5000;

void BME280_Start(){
    Serial1.println(F("BME280 test"));
    
    // default settings
    unsigned status = bme.begin(0x76);  
    // You can also pass in a Wire library object like &Wire2
    // status = bme.begin(0x76, &Wire2)
    if (!status) {
        Serial1.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Serial1.println("SensorID was: 0x"); 
        Serial1.println(bme.sensorID(),16);
        Serial1.println("ID of 0xFF probably means a bad address, a BMP 180 or BMP 085");
        Serial1.println("ID of 0x56-0x58 represents a BMP 280,");
        Serial1.println("ID of 0x60 represents a BME 280.");
        Serial1.println("ID of 0x61 represents a BME 680.");
        while (1) delay(10);
    }
    
    Serial1.println("-- Default Test --\n");
  
}

void getBMEValues() {
  BME280_Temperature = bme.readTemperature();
  BME280_Pressure = bme.readPressure()/ 100.0F;
  BME280_Altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  BME280_Humidity = bme.readHumidity();
  
  Serial1.print("Temperature = ");
  Serial1.print(BME280_Temperature);
  Serial1.print(" *C");

  Serial1.print("\t Humidity = ");
  Serial1.print(BME280_Humidity);
  Serial1.println(" %");

  Serial1.print("Pressure = ");
  Serial1.print(BME280_Pressure);
  Serial1.print(" hPa");

  Serial1.print("\t Approx. Altitude = ");
  Serial1.print(BME280_Altitude);
  Serial1.println(" m");

  Serial1.println();
}

/*
void setup() {
  Serial1.begin(UART_BAUD, SERIAL_8N1, BOARD_RX, BOARD_TX);
  Serial.begin(UART_BAUD);
  while(!Serial & !Serial1);    // time to get serial running
  BME280_Start();
}


void loop() { 
    getBMEValues();
    delay(delayTime);
}
*/