#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <IRremote.h>
#include "auth.h"
#include "GPIO_config.h"
#include "topics.h"
#include "headers.h"

hw_timer_t *watchdogTimer = NULL;
DHT dht(DHT_Pin, DHT_TYPE);
IRsend irSend(IR_SEND_PIN);
int mqttRetryAttempt = 0;
boolean resetCondition = false;
long lastMsg = 0;
long lastMsg2 = 0;
long lastMsg3 = 0;
char msg[20];
char touchmsg[20];
int counter = 0;
DynamicJsonBuffer  jsonBuffer(200);
                  
/* create an instance of WiFiClientSecure */
WiFiClient esp32_WiFiClient;
PubSubClient client(MQTT_SERVER, 1883, &receivedCallback, esp32_WiFiClient);
MFRC522 mfrc522(SS_PIN, RST_PIN);

void connectToWiFi(){
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA); //Station mode (WIFI chip mode) =/= AP: Access point mode
  WiFi.begin(WIFI_NETWORK,WIFI_PASSWORD);

  unsigned long startAttemptTime = millis();

  while(WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_S*1000){
    Serial.print(".");
    delay(500);
  }

  if(WiFi.status() != WL_CONNECTED){
    Serial.println(" Failed to connect");
    reboot();
  }
  else{
    Serial.print("Connected! (IP: ");
    Serial.print(WiFi.localIP());
    Serial.println(")");
  }
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received: ");
  Serial.print(topic);
  Serial.println("");

  Serial.print("payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
 
  if (strcmp(topic,RFID_R_TOPIC)==0) {
    if ((char)payload[0] == '1') {Serial.println("\nAccess granted.\n");} 
    else {Serial.println("Access denied.\n");}
  } 
  
  if (strcmp(topic,R1_TOPIC)==0) {
    if ((char)payload[0] == '1') {digitalWrite(r1, HIGH);} else {digitalWrite(r1, LOW);}
  } 

  else if (strcmp(topic,R2_TOPIC)==0) {
    if ((char)payload[0] == '1') {digitalWrite(r2, HIGH);} else {digitalWrite(r2, LOW);}
  } 

  else if (strcmp(topic,R3_TOPIC)==0) {
    if ((char)payload[0] == '1') {digitalWrite(r3, HIGH);} else {digitalWrite(r3, LOW);}
  }

  else if (strcmp(topic,TV_TOPIC_SONY) == 0) {
      JsonObject& root = jsonBuffer.parseObject(payload);
      if (!root.success()) {
        Serial.println("parseObject() failed");
        return;
      }
      const char* code1 = root["code1"];
      const char* code2 = root["code2"];
      const char* code3 = root["code3"];
      Serial.println(code1);
      if (code1) {
        unsigned long hexCode = strtoul(code1, NULL, 16);
        Serial.println(hexCode, HEX);
        for (int i = 0; i < 3; i++) {
          irSend.sendSony(hexCode, 12);
          delay(40);
        }
      }

      if (code2) {
        unsigned long hexCode = strtoul(code2, NULL, 16);
        Serial.println(hexCode, HEX);
        delay(100);
        for (int i = 0; i < 3; i++) {
          irSend.sendSony(hexCode, 12);
          delay(40);
        }
      }

      if (code3) {
        unsigned long hexCode = strtoul(code3, NULL, 16);
        Serial.println(hexCode, HEX);
        delay(100);
        for (int i = 0; i < 3; i++) {
          irSend.sendSony(hexCode, 12);
          delay(40);
        }
      }
      
  }
}

void connectToBroker() {
  /* Loop until reconnected */
  while (!client.connected()) {
    Serial.print("MQTT connecting ...");
    /* connect now */
    if (client.connect(MQTT_CLIENT_ID, MQTT_SERVER_NAME, MQTT_SERVER_PASSWORD)) {
      Serial.println("connected");
      /* subscribe topic */
      client.subscribe(R1_TOPIC);
      client.subscribe(R2_TOPIC);
      client.subscribe(R3_TOPIC);
      client.subscribe(TV_TOPIC_SONY);
      client.subscribe(RFID_R_TOPIC);
      //client.subscribe(WEATHER_TOPIC);
    } 
    else {
      Serial.print(" failed, status code =");
      Serial.print(client.state());
      Serial.println(", try again in 5 seconds");
      /* Wait 5 seconds before retrying */
      delay(5000);
      mqttRetryAttempt++;
      if (mqttRetryAttempt > 5) {
        Serial.println("Restarting!");
        reboot();
      }
    }
  }
}

void initializeWatchdogTimer(){
  watchdogTimer = timerBegin(0,80,true);
  timerAlarmWrite(watchdogTimer, WATCHDOG_TIMEOUT_S*1000000, false);
  timerAttachInterrupt(watchdogTimer, &interruptReboot, true);
  timerAlarmEnable(watchdogTimer);
}

void reboot(){
  Serial.println("...Rebooting...");
  //ESP.restart();
}

void IRAM_ATTR interruptReboot() { //IRAM_ATTR because RAM is faster than flash
  Serial.println("...Rebooting...");
  ESP.restart();
}

void RFID_verification(){
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {return;}
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {return;}
  long now3 = millis();
  if(now3 - lastMsg3 > 5000){
  lastMsg3 = now3;
  //Show UID on serial monitor
  Serial.print("UID tag :");
  String content= "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  char contento[90];
  content.toUpperCase();
  content = "{\"uid\":" + String("\"") + content.substring(1) + String("\"") + "}";
  content.toCharArray(contento, (content.length() + 1));
  client.publish(RFID_TOPIC, contento, false); //0C 90 43 4A 
  }
}

void setup() {
  Serial.begin(115200);

  /* connexion au wifi */
  connectToWiFi();

  /* Définition des relais comme sortie et initialisation des relais*/
  pinMode(r1, OUTPUT);
  pinMode(r2, OUTPUT);
  pinMode(r3, OUTPUT);
  pinMode(button, INPUT);
  digitalWrite(r1, LOW);  //Or HIGH (Depends on relay)
  digitalWrite(r2, LOW);
  digitalWrite(r3, LOW);
  
  dht.begin(); //capteur de temperature
  SPI.begin();      // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522
  Serial.println("Everything Setup");
}

void loop() {
  /* if client was disconnected then try to reconnect again */
  if (!client.connected()) {
    connectToBroker();
  }
  /* this function will listen for incomming subscribed topic-process-invoke receivedCallback */
  client.loop();

  RFID_verification();
  
  if(digitalRead(button)){
    long now2 = millis();
    if(now2 - lastMsg2 > 5000){
    lastMsg2 = now2;
    Serial.println("Button pr");
    const char* str = "Someone opened the door.";
    client.publish(DOOR_TOPIC, str);
    }
  }   

  /* we increase counter every 3 secs we count until 3 secs reached to avoid blocking program if using delay()*/
  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    char data[90];
    char temp[8];
    char humidity[8];
    dtostrf(dht.readTemperature(),  6, 2, temp);
    dtostrf(dht.readHumidity(), 6, 2, humidity);
    String json = "{\"temperature\":" + String(temp) + ",\"humidity\":" + String(humidity) + "}";
    json.toCharArray(data, (json.length() + 1));
    client.publish(WEATHER_TOPIC, data, false);
  }
  
  unsigned long lastMillis;
  if (!resetCondition)
  {
    // start 1-hour timer;
    lastMillis = millis();
    resetCondition = true;
  } 
  
  if (resetCondition && (millis() - lastMillis >= 3600L * 1000))
  {
    //reboot();
  }

}
