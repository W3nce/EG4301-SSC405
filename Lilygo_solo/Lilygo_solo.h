void light_sleep(uint32_t sec );

void initBME();

String getFormattedDate();

void print_wakeup_reason();
void sendReading(String tempData, String humData, String DateTimeData);

#include "FS.h"
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
bool createDir(fs::FS &fs, const char *path);
bool removeDir(fs::FS &fs, const char *path) ;
bool writeFile(fs::FS &fs, const char *path, const char *message);
bool appendFile(fs::FS &fs, const char *path, const char *message);
bool renameFile(fs::FS &fs, const char *path1, const char *path2);
bool deleteFile(fs::FS &fs, const char *path);
bool writeDataLine(fs::FS &fs, const char *path, const char *messageLine);

void connectWIFI();

void TestWorkflow();
void StartModem();
void StopModem();
void StartGPRS();
void SendPostHttpRequest_TinyGSM();
void SendGetHttpRequest_TinyGSM();
void GetTime_TinyGSM();
void GetTCP();
void GetGPS();
void Arduino_TinyGSM_Test();
