//Device name
#define DEVICE_NAME "DEVICE1";

//Deep Sleep Time/ logging frequency
#define uS_TO_S_FACTOR      1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP       60          /* Time ESP32 will go to sleep (in seconds) */

#define UART_BAUD           115200

//SIM7600G Modem Pinout Configuration
#define MODEM_TX            27
#define MODEM_RX            26
#define MODEM_PWRKEY        4
#define MODEM_DTR           32
#define MODEM_RI            33
#define MODEM_FLIGHT        25
#define MODEM_STATUS        34

//SD Card Pinout COnfiguration
#define SD_MISO             19
#define SD_MOSI             23
#define SD_SCLK             18
#define SD_CS               5

//LED PIN
#define LED_PIN             12 //(LilyGO ESP32 Wrover Mod)
// #define LED_PIN             17 //(ESP32 Wroom32D Dev Mod)

// Replace with your network credentials
#define MY_SSID "SINGTEL-A458"
#define MY_PASSWORD "quoophuuth"

// #define SSID "NOKIA-7480"
// #define PASSWORD "gyL9AJUiLd"
// #define SSID "Terence"
// #define PASSWORD "qwertyios"

//To calculate leap year
#define LEAP_YEAR(Y) ((Y > 0) && !(Y % 4) && ((Y % 100) || !(Y % 400)))

//Logging Temperature in Celcius
#define temperatureCelsius

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

/*
    Tests enabled
*/
#define TINY_GSM_TEST_GPRS          true
// #define TINY_GSM_TEST_TCP           true
// #define TINY_GSM_TEST_CALL          true
// #define TINY_GSM_TEST_SMS           true
// #define TINY_GSM_TEST_USSD          true
// #define TINY_GSM_TEST_TEMPERATURE   true
#define TINY_GSM_TEST_TIME          true
// #define TINY_GSM_TEST_GPS           true

// powerdown modem after tests
// #define TINY_GSM_POWERDOWN          true
// #define TEST_RING_RI_PIN            true

// set GSM PIN, if any
#define GSM_PIN             ""

// Set phone numbers, if you want to test SMS and Calls
// #define SMS_TARGET  "+380xxxxxxxxx"
// #define CALL_TARGET "+380xxxxxxxxx"

// Your GPRS credentials, if any
#define APN "hologram"
// #define APN "ibasis.iot";
#define GPRS_USER ""
#define GPRS_PASS ""

// Server details to test TCP/SSL
#define TARGET_SERVER "vsh.pp.ua"
#define TARGET_RESOURCE "/TinyGSM/logo.txt"
#define DEFAULT_PORT "80"

/////////////////////////////////////////////////////////////////////////////
