#include <ctype.h>

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

bool isValidMacAddress(const char* mac) {
    int i = 0;
    int s = 0;

    while (*mac) {
       if (isxdigit(*mac)) {
          i++;
       }
       else if (*mac == ':' || *mac == '-') {

          if (i == 0 || i / 2 - 1 != s)
            break;

          ++s;
       }
       else {
           s = -1;
       }


       ++mac;
    }

    return (i == 12 && (s == 5 ));
}

bool AuthenticateServer(char * ServerArray){
  std::string AuthCodeRecieved =                      "00000000000000000";
  std::string AuthCode =                              "MACADDRESSAUTHENT";
  std::string OkStatus =                              "11111111111111111";
  std::string FinishStatus =                          "22222222222222222";

  std::string ServerAddress =                         "00000000000000000";
  std::string Emp =                                   "00000000000000000";
  int CharactersRead;

  while (!( CharactersRead == ServerAddressLength 
            && AuthCodeRecieved == AuthCode ) 
            &&  digitalRead(StartLogButton) == HIGH
  ){
    AuthCodeRecieved =                                        "00000000000000000";
    char SerialRecieved[ServerAddressLength + 1] =    "00000000000000000";
    CharactersRead = Serial.readBytesUntil('\n', SerialRecieved,ServerAddressLength);
    AuthCodeRecieved = SerialRecieved;
    // Serial.print("Recieved AuthCode:");
    // Serial.print(SerialRecieved);
    // Serial.println(" (" + String(CharactersRead) + ")");
    Serial.flush();
  }
    
  Serial.println("11111111111111111");

  while (!( CharactersRead == ServerAddressLength 
          && isValidMacAddress(ServerAddress.c_str()))
          &&  digitalRead(StartLogButton) == HIGH
  ){
    ServerAddress =                                   "00000000000000000";
    char SerialRecieved[ServerAddressLength + 1] =    "00000000000000000";
    CharactersRead = Serial.readBytesUntil('\n', SerialRecieved,ServerAddressLength);
    ServerAddress = SerialRecieved;
    // Serial.print("Recieved MAC Address:");
    // Serial.print(SerialRecieved);
    // Serial.println(" (" + String(CharactersRead) + ")");
    Serial.flush();
  }
  
  Serial.println("22222222222222222");

  Serial.print("Recieved MAC Address : ");
  Serial.print(ServerAddress.c_str());
  Serial.println(" (" + String(CharactersRead) + ")");

  
  if (digitalRead(StartLogButton) == LOW){
    Serial.println("LOW");
  } else {
    
    Serial.println("HIGH");
  }

  if (Emp == ServerArray || digitalRead(StartLogButton) == LOW){return false;}
  strcpy(ServerArray, ServerAddress.c_str());

  return true;

}