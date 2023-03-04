//Device name
#define DEVICE_NAME "Parent_InitCONN_LED";

//Deep Sleep Time/ logging frequency
#define uS_TO_S_FACTOR      1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP       30          /* Time ESP32 will go to sleep (in seconds) */

#define UART_BAUD           115200

//SIM7600G Modem Pinout Configuration
// #define MODEM_TX            27
// #define MODEM_RX            26
// #define MODEM_PWRKEY        4
// #define MODEM_DTR           32
// #define MODEM_RI            33
// #define MODEM_FLIGHT        25
// #define MODEM_STATUS        34

//SD Card Pinout COnfiguration
#define SD_MISO             19
#define SD_MOSI             23
#define SD_SCLK             18
#define SD_CS               5

//LED PIN
// #define LED_PIN             12 //(LilyGO ESP32 Wrover Mod)
// #define LED_PIN             17 //(ESP32 Wroom32D Dev Mod)
#define LED_PIN_MAIN              12
#define LED_PIN_GREEN_1           13
#define LED_PIN_GREEN_2           26
#define LED_PIN_GREEN_3           25
#define LED_PIN_GREEN_3           25
#define StartLogButton            14

#define TINY_GSM_MODEM_SIM7600

// Header File for TinyGSM and LED and WIFI and Function prototypes
#include <Ticker.h>
#include <TinyGsmClient.h>
#include <cstring>

// Libraries to connect to WiFi and send Readings to the Cloud
#include <ArduinoHttpClient.h>

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>


void OffAll();
void OnAll();
void flashOnce(int pin_num , int milliSec);
void Blink(int pin_num , int milliSec);
void Bounce();
void print_wakeup_reason();
void Debugf(const char* main, const char* format, bool bswitch);
void Debug(const char* main, bool bswitch);
void Debugln(const char* main, bool bswitch);
void print_wakeup_reason();

// To calculate leap year
#define LEAP_YEAR(Y) ((Y > 0) && !(Y % 4) && ((Y % 100) || !(Y % 400)))

//Logging Temperature in Celcius
// #define temperatureCelsius

////////////////////////////////////////////////////////////////////////////

/*
    MACROS needed for TinyGSM 
*/
#define DEBUG_MODE true

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// #define DUMP_AT_COMMANDS
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// Your GPRS credentials, if any
#define APN "hologram"
// #define APN "ibasis.iot";
#define GPRS_USER ""
#define GPRS_PASS ""

// Server details to test TCP/SSL
#define TARGET_SERVER "sussylogger-2.fly.dev"
#define TARGET_ENDPOINT "/api/master/updateShipment"
#define DEFAULT_PORT "80"
#define CONTENT_TYPE "application/x-www-form-urlencoded"

/////////////////////////////////////////////////////////////////////////////
