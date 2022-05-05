//Function headers
void connectToWiFi();
void receivedCallback(char*, byte*, unsigned int);
void connectToBroker();
void reboot();
void IRAM_ATTR interruptReboot();
void watchdogRefresh();
String ircode (decode_results*);
String encoding (decode_results *results);
void  dumpInfo (decode_results *results);

