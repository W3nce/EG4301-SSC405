#include <string.h>
#include <stdlib.h>
#include "pgmspace.h"
#include <cstdlib>
#include <string>
#include "debug_utils.h"

// Libraries for SD card
#include "FS.h"
#include "SD.h"
#include <SPI.h>

static bool TurnOnSDResults = false;

//SD Card Pinout COnfiguration
#define SD_MISO             19
#define SD_MOSI             23
#define SD_SCLK             18
#define SD_CS               5

bool SD_init(int TotalConnected, bool &isSDInit);
void checkDataCSV(const char *path, bool &isSDInit);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
bool createDir(fs::FS &fs, const char *path);
bool removeDir(fs::FS &fs, const char *path);
bool readFile(fs::FS &fs, const char *path);
bool writeFile(fs::FS &fs, const char *path, const char *message) ;
bool appendFile(fs::FS &fs, const char *path, const char *message);
bool renameFile(fs::FS &fs, const char *path1, const char *path2) ;
bool deleteFile(fs::FS &fs, const char *path);
bool writeDataLine(fs::FS &fs, const char *path, const char *messageLine);
bool readLine(File &f, char* line, size_t maxLen);
bool readVals(File &file , int* v1, float* v2, float* v3, char v4[], int len, float* v5, float* v6);


//Initialises SD Card
bool SD_init(int TotalCSV, bool &isSDInit) {
  Debugln("@SDUtils::SD_Init: Initialise SD CARD",true);
  // Initialise micro-SD Card Module
  if (!SD.begin()) {
    Debugln(">>End:  Card Mount Failed",true);
    return false;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Debugln(">> End: No SD card attached",true);
    return false;
  }

  Debug("\t>> SD Card Type: ",true);
  if (cardType == CARD_MMC) {
    Debugln("MMC",true);
  } else if (cardType == CARD_SD) {
    Debugln("SDSC",true);
  } else if (cardType == CARD_SDHC) {
    Debugln("SDHC",true);
  } else {
    Debugln("UNKNOWN",true);
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("\t>> SD Card Size: %lluMB\n", cardSize);

  Debugln("\t>> Checking if /data.csv and /avrg.csv is ready",true);
  checkDataCSV("/data.csv",isSDInit);
  checkDataCSV("/avrg.csv",isSDInit);

  char * DataCSVPath[] = {"/data1.csv","/data2.csv","/data3.csv"};
  for (int i = 0 ; i < TotalCSV ; i++){
    Debug("\t>> Checking if ",true);
    Debug(DataCSVPath[i], true);
    Debugln(" is ready", true);
    checkDataCSV(DataCSVPath[i], isSDInit);
  }

  Debugln(">> End: SD Card Initialised",true);
  return true;
}

void checkDataCSV(const char * fileName, bool &isSDInit){
  // If the data.txt file doesn't exist
  // Create a file on the SD card and write the data labels
  
  Debugln("@SD_utils::checkDataCSV: Check if CSV file is ready",TurnOnSDResults);
  File file = SD.open(fileName);
  if (!file) {
    Debugln("\t>> File doesn't exist - Creating file...",TurnOnSDResults);
    writeFile(SD, fileName, "Index,Temperature,Humidty,DateTime,Lat,Long\r\n");
  } else {
    Debugln("\t>> File already exists",TurnOnSDResults);
    if (!isSDInit) {
      //delete and recreate?
      deleteFile(SD, fileName);
      writeFile(SD, fileName, "Index,Temperature,Humidity,DateTime,Lat,Long \r\n");
      Debugln("\t>> File Recreated",TurnOnSDResults);
    }
  }
  file.close();  
  Debugln(">> End: CSV File Initialised",TurnOnSDResults);
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
  Serial.printf(">> Reading file: %s\n", path);
  bool status = false;
  File file = fs.open(path);
  if (!file) {
    Serial.println(">> End: Failed to open file for reading");
    file.close();
    return status;
  }

  Serial.print(">> Read from file: \n");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
  Serial.println(">> End: File fully read");
  return true;
}
//Write/Rewrite a File in given directory
bool writeFile(fs::FS &fs, const char *path, const char *message) {
  Debugf(">> Writing file: %s\n", path,TurnOnSDResults);
  bool status = false;
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Debugln(">> End: Failed to open file for writing",true);
    file.close();
    return status;
  }
  if (file.print(message)) {
    Debugf(">> End: %s File written\n", path, true);
    status = true;
  } else {
    Debugf(">> End: %s Write failed\n", path, true);
  }
  file.close();
  return status;
}
//Appends another line in a File in given directory
bool appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);
  bool status = false;

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
  }
  if (file.print(message)) {
    Serial.println("Message appended");
    status = true;
  } else {
    Serial.println("Append failed");
  }
  file.close();
  return status;
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
  Debugf(">> Deleting file: %s\n", path,TurnOnSDResults);
  if (fs.remove(path)) {
    Debugf(">> End: %s File deleted\n", path, true);
    return true;
  } else {
    Debugf(">> End: %s Delete failed\n", path, true);
    return false;
  }
}
//Append a line of Reading in the CSV in the given directory
bool writeDataLine(fs::FS &fs, const char *path, const char *messageLine) {

  Debugf("Writing file: %s\n", path,TurnOnSDResults);
  bool status = false;
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    file.close();
    Debugln("\t\t>> Failed to open file for appending line",true);
    return status;
  }
  if (file.println(messageLine)) {
    Debugln("\t\t>> Line Appended",true);
    status = true;
  } else {
    Debugln("\t\t>> Line Append failed",true);
  }
  file.close();
  return status;
}

bool readLine(File &f, char* line, size_t maxLen) {
  for (size_t n = 0; n < maxLen; n++) {
    int c = f.read();
    if ( c < 0 && n == 0) return false;  // EOF
    if (c < 0 || c == '\n') {
      line[n] = 0;
      return true;
    }
    line[n] = c;
  }
  return false; // line too long
}

bool readVals(File &file , int* v1, float* v2, float* v3, char v4[], int len, float* v5, float* v6) {
  char line[100], *ptr, *str ;
  
  if (!readLine(file, line, sizeof(line))) {
    return false;  // EOF or too long
  }
  ptr = line;
  if (*ptr == '/0'){
    return false;
  }
  *v1 = std::atoi(line);
  while (*ptr) {
      if (*ptr++ == ',') break;
  }
  *v2 = std::strtof(ptr, &str);
  while (*ptr) {
      if (*ptr++ == ',') break;
  }
  *v3 = std::strtof(ptr, &str);
  while (*ptr) {
      if (*ptr++ == ',') break;
  }
  int i = 0;
  while (*ptr && i < len){
      *v4++ = *ptr++;
      i++;
  }
  while (*ptr) {
      if (*ptr++ == ',') break;
  }
  *v5 = std::strtof(ptr, &str);
  while (*ptr) {
      if (*ptr++ == ',') break;
  }
  *v6 = std::strtof(ptr, &str);
  while (*ptr) {
      if (*ptr++ == '\0') break;
  }
  // Serial.println("Read DateTime: " + String(v4));
  return true;  // true if no more Values
}


