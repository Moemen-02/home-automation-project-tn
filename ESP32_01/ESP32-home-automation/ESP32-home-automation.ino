#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <IRremote.h>
#include "auth.h"
#include "GPIO_config.h"
#include "topics.h"
#include "headers.h"

hw_timer_t *watchdogTimer = NULL;
DHT dht(DHT_Pin, DHT_TYPE);
IRrecv irRecv(IR_RECV_PIN);
IRsend irSend(IR_SEND_PIN);
int mqttRetryAttempt = 0;
boolean resetCondition = false;
long lastMsg = 0;
char msg[20];
char touchmsg[20];
int counter = 0;
DynamicJsonBuffer  jsonBuffer(200);
                  
/* create an instance of WiFiClientSecure */
WiFiClient esp32_WiFiClient;
PubSubClient client(MQTT_SERVER, 1883, &receivedCallback, esp32_WiFiClient);

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
  if (strcmp(topic,R1_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r1, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r1, HIGH);
    }
  } else if (strcmp(topic,R2_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r2, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r2, HIGH);
    }
  } else if (strcmp(topic,WEATHER_TOPIC)==0) {
    timerWrite(watchdogTimer, 0);
  } else if (strcmp(topic,R3_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r3, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r3, HIGH);
    }
  } else if (strcmp(topic,R4_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r4, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r4, HIGH);
    }
  } else if (strcmp(topic,R5_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r5, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r5, HIGH);
    }
  } else if (strcmp(topic,R6_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r6, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r6, HIGH);
    }
  } else if (strcmp(topic,R7_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r7, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r7, HIGH);
    }
  } else if (strcmp(topic,R8_TOPIC)==0) {
    if ((char)payload[0] == '1') {
      digitalWrite(r8, LOW);
    } else {
      /* we got '0' -> on */
      digitalWrite(r8, HIGH);
    }
  } else if (strcmp(topic,AV_TOPIC)==0) {
      JsonObject& root = jsonBuffer.parseObject(payload);
      if (!root.success()) {
        Serial.println("parseObject() failed");
        return;
      }
      const char* code1 = root["code1"];
      if (code1) {
        unsigned long hexCode = strtoul(code1, NULL, 16);
        Serial.println(hexCode, HEX);
        irSend.sendNEC(hexCode, 32);
      }
  } else if (strcmp(topic,TV_TOPIC_SONY)==0) {
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
      
  } else if (strcmp(topic,TV_TOPIC_PANASONIC)==0) {
      JsonObject& root = jsonBuffer.parseObject(payload);
      if (!root.success()) {
        Serial.println("parseObject() failed");
        return;
      }
      const char* code1 = root["code1"];
      Serial.println(code1);
      if (code1) {
        unsigned long hexCode = strtoul(code1, NULL, 16);
        Serial.println(hexCode, HEX);
        if (hexCode == 0x100BCBD) {
          for (int i = 0; i < 10; i++) {
            irSend.sendPanasonic(0x4004, hexCode);
            delay(10);
          } 
        } else {
           for (int i = 0; i < 3; i++) {
            irSend.sendPanasonic(0x4004, hexCode);
            delay(40);
          } 
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
      client.subscribe(R4_TOPIC);
      client.subscribe(R5_TOPIC);
      client.subscribe(R6_TOPIC);
      client.subscribe(R7_TOPIC);
      client.subscribe(R8_TOPIC);
      client.subscribe(TV_TOPIC_SONY);
      client.subscribe(TV_TOPIC_PANASONIC);
      client.subscribe(AV_TOPIC);
      client.subscribe(WEATHER_TOPIC);
    } 
    else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
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

void reboot(){
  Serial.println("...Rebooting...");
  ESP.restart();
}

void IRAM_ATTR interruptReboot() { //IRAM_ATTR because RAM is faster than flash
  Serial.println("...Rebooting...");
  ESP.restart();
}

void watchdogRefresh() {
  timerWrite(watchdogTimer, 0); //reset timer (feed watchdog)
}

String ircode (decode_results *results){
  String result;
  // Panasonic has an Address
  if (results->decode_type == PANASONIC) {
    result += "address\":\""+ String(results->address, HEX) + "\",\"";
  }

  result += "\"code\":\"" + String(results->value, HEX) + "\"";
  return result;
}

String encoding (decode_results *results){
  switch (results->decode_type) {
    default:
    case UNKNOWN:      return("UNKNOWN");
    case NEC:          return("NEC");
    case SONY:         return("SONY");
    case RC5:          return("RC5");
    case RC6:          return("RC6");
    case DISH:         return("DISH");
    case SHARP:        return("SHARP");
    case JVC:          return("JVC");
    case SANYO:        return("SANYO");
    case MITSUBISHI:   return("MITSUBISHI");
    case SAMSUNG:      return("SAMSUNG");
    case LG:           return("LG");
    case WHYNTER:      return("WHYNTER");
    case AIWA_RC_T501: return("AIWA_RC_T501");
    case PANASONIC:    return("PANASONIC");
    case DENON:        return("Denon");
  }
}

void  dumpInfo (decode_results *results){
  char data[90];
  String result;
  // Check if the buffer overflowed
  if (results->overflow) {
    result = "{\"message\": \"IR code too long. Edit IRremoteInt.h and increase RAWBUF\"}";
    return;
  }
  String encodingString = String(encoding(results));

  String codeString = String(ircode(results));
  result = "{" + String(ircode(results)) + ",\"encoding\":\"" + String(encoding(results)) + "\",\"bits\":\"" + String(results->bits, DEC) + "\"}";  
  result.toCharArray(data, (result.length()+1));
  Serial.println("publishing");
  client.publish(REMOTE_TOPIC, data, false);
}

void setup() {
  Serial.begin(115200);

  /* Initialisation du 'watchdog' */
  watchdogTimer = timerBegin(0,80,true);
  timerAlarmWrite(watchdogTimer, WATCHDOG_TIMEOUT_S*1000000, false); //old: 40000000
  timerAttachInterrupt(watchdogTimer, &interruptReboot, true);
  timerAlarmEnable(watchdogTimer);

  /* connexion au wifi */
  connectToWiFi();

  /* Définition des relais comme sortie et initialisation des relais*/
  pinMode(r1, OUTPUT);
  pinMode(r2, OUTPUT);
  pinMode(r3, OUTPUT);
  pinMode(r4, OUTPUT);
  pinMode(r5, OUTPUT);
  pinMode(r6, OUTPUT);
  pinMode(r7, OUTPUT);
  pinMode(r8, OUTPUT);
  digitalWrite(r1, LOW);  //Or HIGH (Depends on relay)
  digitalWrite(r2, LOW);
  digitalWrite(r3, LOW);
  digitalWrite(r4, LOW);
  digitalWrite(r5, LOW);
  digitalWrite(r6, LOW);
  digitalWrite(r7, LOW);
  digitalWrite(r8, LOW);
  
  dht.begin();
  irRecv.enableIRIn();

  Serial.println("Everything Setup");
}

void loop() {
  /* if client was disconnected then try to reconnect again */
  if (!client.connected()) {
    connectToBroker();
  }
  /* this function will listen for incomming
    subscribed topic-process-invoke receivedCallback */
  client.loop();
  /* we increase counter every 3 secs
    we count until 3 secs reached to avoid blocking program if using delay()*/
  long now = millis();

   decode_results  results;        // Somewhere to store the results

  if (irRecv.decode(&results)) {  // Grab an IR code
    dumpInfo(&results);           // Output the results
    irRecv.resume();              // Prepare for the next value
  }
  
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
