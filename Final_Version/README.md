Board Used: Lilygo SIM7600G R2 ESP32 WROVER-B/E w/ Accessories (GPS and LTE antenna)
Modules Used: SHT30 breakout module, micro-SD Card reader breakout module, 4 LEDs, 1 START Toggle Button

Full Features: 
1. LED States to see Parent-Child Connections
2. Bluetooth Low Energy Connections (Parent + 3 Child)
3. Logging, Averaging, and Storing in CSV format on micro-SD Card
4. SIM7600G Modem getTime, getLocation, and Post Request
5. Reading from CSV to Post previous unsent readings (Send every N reading saved (Adjustable)/ When there is no network)
6. Deep Sleep (1hr to 8hrs???)
7. Approximate program run time: 
    a. When no network/ not sending readings: ~15 Seconds
    b. When sending readings to cloud: >60 Seconds (Subject number to post requests to make)
    c. Sleep Time: Adjustable (Usually 1hr-8hrs???)

Notes:

 - In the sensor utils header files, child and parent have different UART RX and TX pins

 - Change respectively in utilies.h file:
#define TIME_TO_SLEEP       30        /* Time ESP32 will go to sleep (in seconds) */
#define READING_BEFORE_SEND 2         /* Number of readings before mass posting */
