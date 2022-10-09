#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include "secrets.h"

#define NOTIFY_PIN 3
#define LED_PIN 2

const char* MQTT_CLIENT_ID = "FASTBUTTON01";
const uint16_t MAGIC_NUM = 0x1AC2;
const char* MQTT_ACTION_TOPIC = "wled/buero1";
const char* MQTT_ACTION_VALUE = "T";

//#define DEBUGMODE

struct WIFI_SETTINGS {
  uint16_t magic;
  uint32_t ip_address;
  uint32_t ip_gateway;
  uint32_t ip_mask;
  uint32_t ip_dns1;
  uint32_t ip_dns2;
  char wifi_ssid[50];
  char wifi_auth[50];
  uint8_t wifi_bssid[6];
  uint8_t wifi_channel;
  char mqtt_host_str[50];
  uint32_t mqtt_host_ip;
  int mqtt_host_port;
  char mqtt_user[50];
  char mqtt_auth[50];
} wifi_settings;

void show_connection() {
  #ifdef DEBUGMODE
  Serial.print("WiFi Status - State:  "); Serial.println(WiFi.status()); 
  Serial.print("  IP address:         "); Serial.print(WiFi.localIP());
  Serial.print("  Gateway IP address: "); Serial.println(WiFi.gatewayIP());
  Serial.print("  Subnet mask:        "); Serial.println(WiFi.subnetMask());
  Serial.print("  DNS 0 IP address:   "); Serial.print(WiFi.dnsIP(0));
  Serial.print("  DNS 1 IP address:   "); Serial.print(WiFi.dnsIP(1));
  Serial.print("  DNS 2 IP address:   "); Serial.println(WiFi.dnsIP(2));
  Serial.print("  BSSID:              "); Serial.print(WiFi.BSSIDstr().c_str());
  Serial.print("  Channel: "); Serial.println(WiFi.channel());
  #endif
}

void build_settings(WIFI_SETTINGS *data) {
  // main settings
  data->magic = MAGIC_NUM;
  data->ip_address = WiFi.localIP();
  data->ip_gateway = WiFi.gatewayIP();
  data->ip_mask = WiFi.subnetMask();
  data->ip_dns1 = WiFi.dnsIP(0);
  data->ip_dns2 = WiFi.dnsIP(1);
  strncpy(data->wifi_ssid, WIFI_SSID, 50);
  strncpy(data->wifi_auth, WIFI_AUTH, 50);
  memcpy(data->wifi_bssid, WiFi.BSSID(), 6);
  data->wifi_channel = WiFi.channel();
  // lookup IP for mqtt server
  strncpy(data->mqtt_host_str, MQTT_SERVER, 50);
  IPAddress mqtt_ip;
  int err = WiFi.hostByName(MQTT_SERVER, mqtt_ip);
  if (err==0) { 
    #ifdef DEBUGMODE
    Serial.print(" ** Can't resolve host "); Serial.println(MQTT_SERVER); 
    #endif
  }
  memcpy(&data->mqtt_host_ip, mqtt_ip, 4);
  strncpy(data->mqtt_auth, MQTT_AUTH, 50);
  strncpy(data->mqtt_user, MQTT_USER, 50);
  data->mqtt_host_port = MQTT_SERVER_PORT;
}

void save_settings_to_flash(WIFI_SETTINGS data) {
  EEPROM.begin(512);
  EEPROM.put(0, data);
  EEPROM.end();
}

int get_settings_from_flash() { // dunno why not parameters
  EEPROM.begin(512);
  EEPROM.get(0, wifi_settings);
  EEPROM.end();
  return (wifi_settings.magic == MAGIC_NUM);
}

void display_settings(WIFI_SETTINGS data) {
  #ifdef DEBUGMODE
  char buf[100];
  Serial.println("Settings:");
  Serial.print("Magic:       "); sprintf(buf, "%04X", data.magic); Serial.println(buf);
  Serial.print("Local IP:    "); sprintf(buf, "%08X", data.ip_address); Serial.println(buf);
  Serial.print("Gateway IP:  "); sprintf(buf, "%08X", data.ip_gateway); Serial.println(buf);
  Serial.print("Mask:        "); sprintf(buf, "%08X", data.ip_mask); Serial.println(buf);
  Serial.print("DNS 1 IP:    "); sprintf(buf, "%08X", data.ip_dns1); Serial.println(buf);
  Serial.print("DNS 2 IP:    "); sprintf(buf, "%08X", data.ip_dns2); Serial.println(buf);
  Serial.print("Wifi SSID:   "); Serial.println(data.wifi_ssid);
  Serial.print("Wifi Auth:   "); Serial.println(data.wifi_auth);
  Serial.print("Wifi BSSID:  "); 
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", 
    data.wifi_bssid[0], data.wifi_bssid[1], data.wifi_bssid[2], 
    data.wifi_bssid[3], data.wifi_bssid[4], data.wifi_bssid[5]); Serial.println(buf);
  Serial.print("Wifi Channel:"); sprintf(buf, "%d", data.wifi_channel); Serial.println(buf);
  Serial.print("MQTT Host:   "); Serial.println(data.mqtt_host_str);
  Serial.print("MQTT IP:     "); sprintf(buf, "%08X", data.mqtt_host_ip); Serial.println(buf);
  Serial.print("MQTT Port:   "); sprintf(buf, "%d", data.mqtt_host_port); Serial.println(buf);
  Serial.print("MQTT User:   "); Serial.println(data.mqtt_user);
  Serial.print("MQTT Pass:   "); Serial.println(data.mqtt_auth);
  #endif
}

int wifi_slow_connect() {
  #define SLOW_TIMEOUT 10000 // ms
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_AUTH);
  uint32_t timeout = millis() + SLOW_TIMEOUT;
  while ((WiFi.status() != WL_CONNECTED) && (millis()<timeout)) { delay(10); }
  return (WiFi.status() == WL_CONNECTED);
}

int wifi_fast_connect(WIFI_SETTINGS data) {
  #define FAST_TIMEOUT 5000 // ms
  // try fast connect
  WiFi.config(IPAddress(data.ip_address),
    IPAddress(data.ip_gateway), IPAddress(data.ip_mask), 
    IPAddress(data.ip_dns1), IPAddress(data.ip_dns2));
  WiFi.begin(data.wifi_ssid, data.wifi_auth, 
    data.wifi_channel, data.wifi_bssid);
  // wait for connection
  uint32_t timeout = millis() + FAST_TIMEOUT;
  //while ((WiFi.status() != WL_CONNECTED) && (millis()<timeout)) { delay(5); }
  // note: connection is actually not yet made
  WiFi.reconnect();
  timeout = millis() + FAST_TIMEOUT;
  while ((WiFi.status() != WL_CONNECTED) && (millis()<timeout)) { delay(5); }
  // should be good now
  return (WiFi.status() == WL_CONNECTED);
}

int preconnect_ip(WiFiClient wclient, IPAddress ip, int port) {
  #define PRECONNECT_TIMEOUT 5000
  uint32_t timeout = millis() + PRECONNECT_TIMEOUT;
  while ((!wclient.connect(ip, port)) && (millis()<timeout)) { delay(50); }
  return (wclient.connected());
}

int publish_mqtt(WiFiClient& wclient, WIFI_SETTINGS data, char *topic, char *value, 
    int includestate=0, int autodiscover=0) {
  // no timeouts no ragrets
  PubSubClient mqtt_client(wclient);
  mqtt_client.setServer(data.mqtt_host_ip, data.mqtt_host_port);
  int status = false;
  if (mqtt_client.connect(MQTT_CLIENT_ID, data.mqtt_user, data.mqtt_auth)) {
    #ifdef DEBUGMODE
    Serial.print(" T[");Serial.print(topic);
    Serial.print("]="); Serial.print(value); Serial.print(" ");
    #endif
    if (strlen(topic)>1) {
      mqtt_client.publish(topic, value);
    }
    if (includestate || autodiscover) { 
      char mytopic[200];
      strcpy(mytopic, "softplus/"); strcat(mytopic, MQTT_CLIENT_ID);
      strcat(mytopic, "/state");
      if (autodiscover) {
          char mybuf[500], autotopic[200];
          strcpy(mybuf, "{\"stat_t\":\"");
          strcat(mybuf, mytopic);
          strcat(mybuf, "\",\"name\":\"");
          strcat(mybuf, MQTT_CLIENT_ID);
          strcat(mybuf, "\",\"off_delay\":30,\"dev\":{");
          strcat(mybuf, "\"name\":\"fastbutton\",\"mdl\":\"");
          strcat(mybuf, MQTT_CLIENT_ID);
          strcat(mybuf, "\",\"ids\":\"");
          strcat(mybuf, MQTT_CLIENT_ID);
          strcat(mybuf, "\"}}");
          strcpy(autotopic, "homeassistant/binary_sensor/");
          strcat(autotopic, MQTT_CLIENT_ID);
          strcat(autotopic, "/config");
          #ifdef DEBUGMODE
          Serial.print(" T[");Serial.print(autotopic);
          Serial.print("]="); Serial.print(mybuf); Serial.print(" ");
          #endif
          mqtt_client.publish(autotopic, mybuf);
          // more deets, ip
          strcpy(autotopic, "softplus/"); strcat(autotopic, MQTT_CLIENT_ID);
          strcat(autotopic, "/ip");
          strcpy(mybuf, WiFi.localIP().toString().c_str());
          #ifdef DEBUGMODE
          Serial.print(" T[");Serial.print(autotopic);
          Serial.print("]="); Serial.print(mybuf); Serial.print(" ");
          #endif
          mqtt_client.publish(autotopic, mybuf);
          // mac
          strcpy(autotopic, "softplus/"); strcat(autotopic, MQTT_CLIENT_ID);
          strcat(autotopic, "/mac");
          strcpy(mybuf, WiFi.macAddress().c_str());
          #ifdef DEBUGMODE
          Serial.print(" T[");Serial.print(autotopic);
          Serial.print("]="); Serial.print(mybuf); Serial.print(" ");
          #endif
          mqtt_client.publish(autotopic, mybuf);
      }
      if (includestate) {
        #ifdef DEBUGMODE
        Serial.print(" T[");Serial.print(mytopic);
        Serial.print("]="); Serial.print("ON"); Serial.print(" ");
        #endif
        mqtt_client.publish(mytopic, "ON");
      }
    }
    status = true;
  } else {
    status = false;
  }
  return status;
}

void countdown(int secs) {
  // just some blinking to show we're alive
  pinMode(LED_PIN, OUTPUT);
  #ifdef DEBUGMODE
  Serial.print("Wait: ");
  #endif
  for (int i=secs*2; i>=0; i--) {
    #ifdef DEBUGMODE
    if (i%2==0) { Serial.print(i/2); Serial.print(" "); }
    #endif
    digitalWrite(LED_PIN, ((i%2)==0)?LOW:HIGH);
    delay(500);
  }
}

int slow_rebuild() {
  int working = 0;
  #ifdef DEBUGMODE
  Serial.print(" wifi_slow_connect ");
  #endif
  if (!wifi_slow_connect()) {
    working = false; // we failed. sad
    #ifdef DEBUGMODE
    Serial.println(" Failed ");
    #endif
  } else {
    #ifdef DEBUGMODE
    Serial.print(" build_settings ");
    #endif
    build_settings(&wifi_settings);
    #ifdef DEBUGMODE
    Serial.print(" save_settings_to_flash ");
    #endif
    save_settings_to_flash(wifi_settings);
    #ifdef DEBUGMODE
    Serial.print(" display_settings ");
    display_settings(wifi_settings);
    Serial.println(" OK ");
    #endif
    working = true;
  }
  return working;
}

void setup() {
  #ifndef DEBUGMODE
  pinMode(NOTIFY_PIN, OUTPUT); 
  digitalWrite(NOTIFY_PIN, HIGH); 
  #endif
  WiFi.setAutoConnect(false);

  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(LED_PIN, LOW); // LED on

  #ifdef DEBUGMODE
  Serial.begin(115200);
  Serial.print("Starting soon...");
  countdown(4);
  Serial.print("Millis="); Serial.print(millis()); Serial.print(" ms");
  uint32_t start_time_all = millis();
  #endif

  int working = false, register_mqtt = false;

  #ifdef DEBUGMODE
  Serial.println();
  Serial.print("WIFI: ");
  Serial.print(" get_settings_from_flash ");
  #endif
  if (!get_settings_from_flash()) {
    #ifdef DEBUGMODE
    Serial.print(" wifi_slow_connect ");
    #endif
    if (!wifi_slow_connect()) {
      working = false; // we failed. sad
      #ifdef DEBUGMODE
      Serial.println(" Failed ");
      #endif
    } else {
      #ifdef DEBUGMODE
      Serial.print(" build_settings ");
      #endif
      build_settings(&wifi_settings);
      register_mqtt = true;
      #ifdef DEBUGMODE
      Serial.print(" save_settings_to_flash ");
      #endif
      save_settings_to_flash(wifi_settings);
      #ifdef DEBUGMODE
      Serial.print(" display_settings ");
      display_settings(wifi_settings);
      Serial.println(" OK ");
      #endif
      working = true;
    }
  } else {
    // try fast-connect
    #ifdef DEBUGMODE
    Serial.print(" display_settings ");
    display_settings(wifi_settings);
    Serial.print(" wifi_fast_connect ");
    #endif
    if (!wifi_fast_connect(wifi_settings)) { 
      // nope, revert to slow
      #ifdef DEBUGMODE
      Serial.print(" wifi_slow_connect ");
      #endif
      if (!wifi_slow_connect()) {
        working = false; // we failed. sad
        #ifdef DEBUGMODE
        Serial.println(" Failed ");
        #endif
      } else {
        // save settings
        #ifdef DEBUGMODE
        Serial.print(" build_settings ");
        #endif
        build_settings(&wifi_settings);
        #ifdef DEBUGMODE
        Serial.print(" save_settings_to_flash ");
        #endif
        save_settings_to_flash(wifi_settings);
        working = true;
        #ifdef DEBUGMODE
        Serial.println(" OK ");
        #endif
      }
    } else {
      working = true;
      #ifdef DEBUGMODE
      Serial.println(" OK ");
      #endif
    }
  }

  #ifdef DEBUGMODE
  Serial.println();
  Serial.print("MQTT: ");
  #endif

  int worked = false;
  if (working) {
    WiFiClient wclient;
    #ifdef DEBUGMODE
    show_connection();
    Serial.print(" preconnect_ip ");
    #endif
    if (preconnect_ip(wclient, wifi_settings.mqtt_host_ip, wifi_settings.mqtt_host_port)) {
      #ifdef DEBUGMODE
      Serial.print(" publish_mqtt ");
      #endif
      char topic[100]; strncpy(topic, MQTT_ACTION_TOPIC, 100);
      char value[100]; strncpy(value, MQTT_ACTION_VALUE, 100);
      if (publish_mqtt(wclient, wifi_settings, topic, value, true, register_mqtt)) {
          worked = true;
          #ifdef DEBUGMODE
          Serial.println(" OK ");
          #endif
      } else {
        worked = false;
        #ifdef DEBUGMODE
        Serial.println(" Failed ");
        #endif
      }
    } else {
      worked = false;
      #ifdef DEBUGMODE
      Serial.println(" Failed ");
      #endif
    }
  }
  #ifdef DEBUGMODE
  Serial.print("Result: ");
  if (worked) Serial.println("OK"); else Serial.println("FAILED");

  Serial.println();
  Serial.print("Duration: "); 
  Serial.print((millis()-start_time_all)); 
  Serial.println(" ms");
  #endif
}

void loop() {
  //#define REBUILD_YES
  #ifdef REBUILD_YES
    Serial.print("Rebuilding: ");
    countdown(5);
    uint32_t start_time_all = millis();
    slow_rebuild();
    Serial.println();
    Serial.print("Duration: "); 
    Serial.print((millis()-start_time_all)/(TEST_COUNT)); 
    Serial.println(" ms");
  #endif
  #ifndef DEBUGMODE
  for (int i=0; i<10; i++) {
    digitalWrite(LED_PIN, ((i%2)==0)?LOW:HIGH); delay(150);
  }
  digitalWrite(LED_PIN, HIGH); // LED off
  digitalWrite(NOTIFY_PIN, LOW); // should power down
  delay(4000); // wait a little
  // if still here, rebuild it all
  digitalWrite(LED_PIN, LOW); // LED on
  digitalWrite(NOTIFY_PIN, HIGH); // keep power up now
  delay(100);
  slow_rebuild();
  WiFiClient wclient;
  if (preconnect_ip(wclient, wifi_settings.mqtt_host_ip, wifi_settings.mqtt_host_port)) {
    char topic[100]; topic[0]=0;
    char value[100]; value[0]=0;
    publish_mqtt(wclient, wifi_settings, topic, value, false, true);
  }
  digitalWrite(LED_PIN, HIGH); // LED off
  delay(1000); // wait a little
  digitalWrite(NOTIFY_PIN, LOW); // power down again
  ESP.deepSleep(30e6); // this actually never wakes up, muhahaha
  #endif
  #ifdef DEBUGMODE
  countdown(4);
  Serial.print("Restarting: ");
  ESP.restart();
  Serial.println("Should not be here.");
  #endif
}
