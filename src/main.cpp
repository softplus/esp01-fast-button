#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <PubSubClient.h>

// secrets.h contains WIFI and MQTT credentials
#include "secrets.h"

#define NOTIFY_PIN 3
#define LED_PIN 2

//const char* MQTT_CLIENT_ID = "FASTBUTTON02";
//const char* MQTT_ACTION_TOPIC = "wled/buero1";
//const char* MQTT_ACTION_VALUE = "T";

//#define DEBUG_MODE
//#define DEBUG_AUTODISCOVER

#ifdef DEBUG_MODE
#define DEBUG_LOG(x) Serial.print(F(x)); Serial.print(F(" @ ")); Serial.println(millis()) 
#else
#define DEBUG_LOG(x) 
#endif

/* Our data structure for WIFI settings */
const uint16_t MAGIC_NUM = 0x1AC4;
struct WIFI_SETTINGS_T { // size: 640 bytes
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
  uint16_t mqtt_host_port;
  char mqtt_user[50];
  char mqtt_auth[50];
  char mqtt_client_id[50];
  char mqtt_topic[100];
  char mqtt_value[100];
  char mqtt_homeassistant_topic[100];
} g_wifi_settings;
bool g_wifi_mqtt_working;
unsigned long g_start_millis;
bool g_mqtt_connected;
PubSubClient g_mqtt_client;
WiFiClient g_wclient;

// functions that follow
bool wifi_try_slow_connect(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w);
bool wifi_slow_connect(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w);
bool wifi_try_fast_connect(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w);
void save_settings_to_flash(WIFI_SETTINGS_T *data);
bool get_settings_from_flash(WIFI_SETTINGS_T *data);
void default_settings(WIFI_SETTINGS_T *data);
void build_settings_from_wifi(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w);
void set_settings_ap(WIFI_SETTINGS_T *data, char *ssid, char *auth);
void show_wifi_info(ESP8266WiFiClass *w);
void show_settings(WIFI_SETTINGS_T *data);
void countdown(int secs);
bool mqtt_connect_server(WiFiClient *wclient, WIFI_SETTINGS_T *data);
bool mqtt_send_topic(char *topic, char *value);
bool mqtt_send_autodiscover(WIFI_SETTINGS_T *data);
bool mqtt_send_network_info(ESP8266WiFiClass *w, WIFI_SETTINGS_T *data);
bool mqtt_send_device_state(WIFI_SETTINGS_T *data);

/* Main setup() function: 
 *  1. Pulls NOTIFY_PIN high (keeps power on)
 *  2. If no cached wifi data: do a traditional connect & cache
 *  3. Alternately: try fast wifi connect, or traditional connect
 *  4. Connect to MQTT server
 *  5. Send MQTT packets, including Home-Assistant autodiscovery
 *  6. (MCU continues with loop() below)
 */
void setup() {
  #ifndef DEBUG_MODE
  pinMode(NOTIFY_PIN, OUTPUT); 
  digitalWrite(NOTIFY_PIN, HIGH); 
  #endif
  WiFi.setAutoConnect(false);

  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(LED_PIN, LOW); // LED on

  #ifdef DEBUG_MODE
  Serial.begin(115200);
  Serial.print("Starting soon...");
  countdown(4);
  #endif
  g_start_millis = millis();

  g_wifi_mqtt_working = false; // assume the worst
  g_mqtt_connected = false; // MQTT is also not connected
  bool autodiscover_mqtt = false;

  DEBUG_LOG("\n## WIFI:");
  if (!get_settings_from_flash(&g_wifi_settings)) {
    default_settings(&g_wifi_settings);
    g_wifi_mqtt_working = wifi_try_slow_connect(&g_wifi_settings, &WiFi);
    autodiscover_mqtt = true;
  } else {
    // try fast-connect
    #ifdef DEBUG_MODE
    show_settings(&g_wifi_settings);
    #endif
    if (!wifi_try_fast_connect(&g_wifi_settings, &WiFi)) { 
      // nope, revert to slow
      g_wifi_mqtt_working = wifi_try_slow_connect(&g_wifi_settings, &WiFi);
      autodiscover_mqtt = true;
    } else {
      g_wifi_mqtt_working = true;
    }
  }
  #ifdef DEBUG_AUTODISCOVER
  autodiscover_mqtt = true;
  #endif

  DEBUG_LOG("\n## MQTT:");
  if (g_wifi_mqtt_working) {
    #ifdef DEBUG_MODE
    show_wifi_info(&WiFi);
    #endif
    if (!mqtt_connect_server(&g_wclient, &g_wifi_settings)) {
      DEBUG_LOG("mqtt_connect_server() FAILED");
      g_wifi_mqtt_working = false;
    } else {
      if (!mqtt_send_topic(g_wifi_settings.mqtt_topic, g_wifi_settings.mqtt_value)) {
        DEBUG_LOG("mqtt_send_topic(main) FAILED");
        g_wifi_mqtt_working = false;
      } else {
        if (autodiscover_mqtt) {
          mqtt_send_autodiscover(&g_wifi_settings);
          mqtt_send_network_info(&WiFi, &g_wifi_settings);
        }
        mqtt_send_device_state(&g_wifi_settings);
      }
    }
  }
  #ifdef DEBUG_MODE
  Serial.print("Result: ");
  if (g_wifi_mqtt_working) Serial.println("OK"); else Serial.println("FAILED");

  Serial.println();
  Serial.print("Duration: "); 
  Serial.print((millis()-g_start_millis)); 
  Serial.println(" ms");
  #endif
  DEBUG_LOG("\n## setup() complete");
}

/* Main processing loop():
 *  1. Blink 1.5 seconds at 5Hz
 *  2. Pull NOTIFY_PIN down (power off)
 *  3. If someone is still pusing power, rebuild the Wifi cache
 *  4. Go into deep sleep
 */
void loop() {
  DEBUG_LOG("\n#  loop()");

  #ifndef DEBUG_MODE
  for (int i=0; i<1500/100; i++) {
    digitalWrite(LED_PIN, ((i%2)==0)?LOW:HIGH); delay(100);
  }
  digitalWrite(LED_PIN, HIGH); // LED off
  digitalWrite(NOTIFY_PIN, LOW); // should power down
  delay(4000); // wait a little
  // if still here, rebuild it all
  digitalWrite(LED_PIN, LOW); // LED on
  digitalWrite(NOTIFY_PIN, HIGH); // keep power up now
  delay(100);
  // disconnect MQTT, reconnect Wifi, cache state, do autodiscovery
  if (g_mqtt_connected) g_mqtt_client.disconnect();
  bool res = wifi_try_slow_connect(&g_wifi_settings, &WiFi);
  if (res) {
    if (mqtt_connect_server(&g_wclient, &g_wifi_settings)) {
      mqtt_send_network_info(&WiFi, &g_wifi_settings);
      mqtt_send_autodiscover(&g_wifi_settings);
    }
  }
  // complete
  digitalWrite(LED_PIN, HIGH); // LED off
  delay(1000); // wait a little
  digitalWrite(NOTIFY_PIN, LOW); // power down again
  ESP.deepSleep(30e6); // this actually never wakes up, muhahaha
  #endif

  #ifdef DEBUG_MODE
  countdown(4);
  DEBUG_LOG("ESP.restart() ...");
  ESP.restart();
  DEBUG_LOG("... FAILED ... you should never be here");
  ESP.deepSleep(30e6); 
  #endif
  // never does second loop
}

/* WIFI Connection ------------------------------------------------- */
/* ----------------------------------------------------------------- */

/* Connect to the AP using traditional SSID, AUTH
 */
bool wifi_slow_connect(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w) {
  DEBUG_LOG("wifi_slow_connect()");

  #define SLOW_TIMEOUT 10000 // ms
  w->mode(WIFI_STA);
  w->begin(data->wifi_ssid, data->wifi_auth);
  uint32_t timeout = millis() + SLOW_TIMEOUT;
  while ((WiFi.status() != WL_CONNECTED) && (millis()<timeout)) { delay(10); }
  return (WiFi.status() == WL_CONNECTED);
}

/* Try a slow connection 
 * returns true if connected, false if failure 
 */
bool wifi_try_slow_connect(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w) {
  DEBUG_LOG("wifi_try_slow_connect()");

  set_settings_ap(data, (char *)WIFI_SSID, (char *)WIFI_AUTH);
  if (!wifi_slow_connect(data, w)) {
    DEBUG_LOG("wifi_slow_connect() FAILED");
    return false;
  }

  build_settings_from_wifi(data, w);
  save_settings_to_flash(data);
  #ifdef DEBUG_MODE
  show_settings(data);
  #endif
  return true;
}

/* Attempt to connect to the AP using our cached AP data
 */
bool wifi_try_fast_connect(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w) {
  DEBUG_LOG("wifi_try_fast_connect()");

  #define FAST_TIMEOUT 5000 // ms
  // try fast connect
  w->config(IPAddress(data->ip_address),
    IPAddress(data->ip_gateway), IPAddress(data->ip_mask), 
    IPAddress(data->ip_dns1), IPAddress(data->ip_dns2));
  w->begin(data->wifi_ssid, data->wifi_auth, data->wifi_channel, data->wifi_bssid);
  // note: connection is actually not yet made
  w->reconnect();
  // wait for connection, or time out
  uint32_t timeout = millis() + FAST_TIMEOUT;
  while ((w->status() != WL_CONNECTED) && (millis()<timeout)) { delay(5); }
  // should be good now, or timed out
  return (w->status() == WL_CONNECTED);
}

/* Save & restore settings from Flash ------------------------------ */
/* ----------------------------------------------------------------- */

/* Saves our wifi settings structure to flash memory
 * using the EEPROM library.
 */
void save_settings_to_flash(WIFI_SETTINGS_T *data) {
  DEBUG_LOG("save_settings_to_flash()");

  char buf[sizeof(*data)];
  memcpy(&buf, data, sizeof(buf));
  EEPROM.begin(sizeof(buf));
  EEPROM.put(0, buf);
  EEPROM.commit();
  EEPROM.end();
}

/* Fetches settings from flash 
*/
bool get_settings_from_flash(WIFI_SETTINGS_T *data) {
  DEBUG_LOG("get_settings_from_flash()");

  char buf[sizeof(*data)];
  EEPROM.begin(sizeof(buf));
  EEPROM.get(0, buf);
  EEPROM.end();
  memcpy(data, &buf, sizeof(buf));

  #ifdef DEBUG_MODE
  char b[10]; // display first part of settings for confirmation, if debugging
  Serial.print(F(" peek into data: "));
  char *d = (char *)data;
  for (int i=0; i<16; i++) {
    snprintf(b,sizeof(b), "%02x ", *d++); Serial.print(b);
  }
  Serial.println();
  #endif

  return (data->magic == MAGIC_NUM);
}

/* Creates default settings structure
 */
void default_settings(WIFI_SETTINGS_T *data) {
  DEBUG_LOG("default_settings()");

  // main settings
  memset(data, 0, sizeof(*data));
  data->magic = MAGIC_NUM;
  strncpy(data->mqtt_host_str, "homeassistant.local", sizeof(data->mqtt_host_str));
  data->mqtt_host_port = 1883;
  strncpy(data->mqtt_user, "username", sizeof(data->mqtt_user));
  strncpy(data->mqtt_auth, "password", sizeof(data->mqtt_auth));
  strncpy(data->mqtt_client_id, "FASTBUTTON", sizeof(data->mqtt_client_id));
  strncpy(data->mqtt_topic, "wled/lights", sizeof(data->mqtt_topic));
  strncpy(data->mqtt_value, "T", sizeof(data->mqtt_value));
}

/* Stores settings from global WiFi object to linked data
 * structure, fetches IP address of MQTT server.
 */
void build_settings_from_wifi(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w) {
  DEBUG_LOG("build_settings_from_wifi()");

  // main settings
  data->ip_address = w->localIP();
  data->ip_gateway = w->gatewayIP();
  data->ip_mask = w->subnetMask();
  data->ip_dns1 = w->dnsIP(0);
  data->ip_dns2 = w->dnsIP(1);
  strncpy(data->wifi_ssid, WIFI_SSID, sizeof(data->wifi_ssid));
  strncpy(data->wifi_auth, WIFI_AUTH, sizeof(data->wifi_auth));
  memcpy(data->wifi_bssid, w->BSSID(), 6);
  data->wifi_channel = w->channel();
  // look up IP for MQTT server
  IPAddress mqtt_ip;
  int err = w->hostByName(data->mqtt_host_str, mqtt_ip);
  if (err==0) { 
    data->mqtt_host_ip = 0;
    #ifdef DEBUG_MODE
    Serial.print(" ** Can't resolve host: "); Serial.println(MQTT_SERVER); 
    #endif
  } else {
    data->mqtt_host_ip = (uint32_t)mqtt_ip;
  }
  /*
  strncpy(data->mqtt_auth, MQTT_AUTH, sizeof(data->mqtt_auth));
  strncpy(data->mqtt_user, MQTT_USER, sizeof(data->mqtt_user));
  data->mqtt_host_port = MQTT_SERVER_PORT;
  strncpy(data->mqtt_client_id, MQTT_CLIENT_ID, sizeof(data->mqtt_client_id));
  strncpy(data->mqtt_topic, MQTT_ACTION_TOPIC, sizeof(data->mqtt_topic));
  strncpy(data->mqtt_value, MQTT_ACTION_VALUE, sizeof(data->mqtt_value));
  */
}

/* Save AP SSID & AUTH to data structure */
void set_settings_ap(WIFI_SETTINGS_T *data, char *ssid, char *auth) {
  DEBUG_LOG("wifi_set_ap()");

  strncpy(data->wifi_ssid, ssid, sizeof(data->wifi_ssid));
  data->wifi_ssid[sizeof(data->wifi_ssid)-1] = 0;
  strncpy(data->wifi_auth, auth, sizeof(data->wifi_auth));
  data->wifi_auth[sizeof(data->wifi_auth)-1] = 0;
}

/* Show some debugging information --------------------------------- */
/* ----------------------------------------------------------------- */

/* Show information about the global WiFi object, if we're in debug-mode */
void show_wifi_info(ESP8266WiFiClass *w) {
  DEBUG_LOG("show_wifi_info()");

  #ifdef DEBUG_MODE
  Serial.print("WiFi Status - State:  "); Serial.println(w->status()); 
  Serial.print("  IP address:         "); Serial.print(w->localIP());
  Serial.print("  Gateway IP address: "); Serial.println(w->gatewayIP());
  Serial.print("  Subnet mask:        "); Serial.println(w->subnetMask());
  Serial.print("  DNS 0 IP address:   "); Serial.print(w->dnsIP(0));
  Serial.print("  DNS 1 IP address:   "); Serial.print(w->dnsIP(1));
  Serial.print("  DNS 2 IP address:   "); Serial.println(w->dnsIP(2));
  Serial.print("  BSSID:              "); Serial.print(w->BSSIDstr().c_str());
  Serial.print("  Channel: "); Serial.println(w->channel());
  #endif
}

/* If we're debugging, show the full settings
 */
void show_settings(WIFI_SETTINGS_T *data) {
  DEBUG_LOG("show_settings()");

  #ifdef DEBUG_MODE
  char buf[100];
  Serial.println("Settings:");
  snprintf(buf, sizeof(buf), "Magic:        %04X", data->magic); Serial.println(buf);
  snprintf(buf, sizeof(buf), "Local IP:     %08X", data->ip_address); Serial.println(buf);
  snprintf(buf, sizeof(buf), "Gateway IP:   %08X", data->ip_gateway); Serial.println(buf);
  snprintf(buf, sizeof(buf), "Mask:         %08X", data->ip_mask); Serial.println(buf);
  snprintf(buf, sizeof(buf), "DNS 1 IP:     %08X", data->ip_dns1); Serial.println(buf);
  snprintf(buf, sizeof(buf), "DNS 2 IP:     %08X", data->ip_dns2); Serial.println(buf);
  snprintf(buf, sizeof(buf), "Wifi SSID:    %s", data->wifi_ssid); Serial.println(buf);
  snprintf(buf, sizeof(buf), "Wifi Auth:    %s", data->wifi_auth); Serial.println(buf);
  snprintf(buf, sizeof(buf), "Wifi BSSID:   "
    "%02X:%02X:%02X:%02X:%02X:%02X", 
    data->wifi_bssid[0], data->wifi_bssid[1], data->wifi_bssid[2], 
    data->wifi_bssid[3], data->wifi_bssid[4], data->wifi_bssid[5]); Serial.println(buf);
  snprintf(buf, sizeof(buf), "Wifi Channel: %d", data->wifi_channel); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT Host:    %s", data->mqtt_host_str); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT IP:      %08X", data->mqtt_host_ip); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT Port:    %d", data->mqtt_host_port); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT User:    %s", data->mqtt_user); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT Pass:    %s", data->mqtt_auth); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT ClientID:%s", data->mqtt_client_id); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT Topic:   %s", data->mqtt_topic); Serial.println(buf);
  snprintf(buf, sizeof(buf), "MQTT Value:   %s", data->mqtt_value); Serial.println(buf);
  #endif
}

/* Waits a bit and blinks at 1Hz, to show we've done something */
void countdown(int secs) {
  DEBUG_LOG("countdown()");

  pinMode(LED_PIN, OUTPUT);
  for (int i=secs*2; i>=0; i--) {
    #ifdef DEBUG_MODE
    if (i%2==0) { Serial.print(i/2); Serial.print(" "); }
    #endif
    digitalWrite(LED_PIN, ((i%2)==0)?LOW:HIGH);
    delay(500);
  }
  DEBUG_LOG("");
}

/* MQTT  ----------------------------------------------------------- */
/* ----------------------------------------------------------------- */

/* Try to connect to MQTT server, if needed
 */
bool mqtt_connect_server(WiFiClient *wclient, WIFI_SETTINGS_T *data) {
  DEBUG_LOG("mqtt_connect_server()");
  if (g_mqtt_connected) return true;

  // Pre-connect to IP address
  #define PRECONNECT_TIMEOUT 5000
  uint32_t timeout = millis() + PRECONNECT_TIMEOUT;
  while ((!wclient->connect(
      data->mqtt_host_ip, data->mqtt_host_port)) && (millis()<timeout)) { delay(50); }
  if (!wclient->connected()) {
    DEBUG_LOG("Connect to MQTT IP-address FAILED");
    return false; // can't connect to IP
  }

  // Do full connection to MQTT
  g_mqtt_client.setClient(*wclient);
  g_mqtt_client.setServer(data->mqtt_host_ip, data->mqtt_host_port);

  if (!g_mqtt_client.connect(data->mqtt_client_id, data->mqtt_user, data->mqtt_auth)) {
    DEBUG_LOG("MQTT.connect() FAILED");
    return false;
  }
  g_mqtt_connected = true;
  return true;
}

/* Publish a topic to MQTT, if connected
 */
bool mqtt_send_topic(char *topic, char *value) {
  DEBUG_LOG("mqtt_send_topic()");
  if (!g_mqtt_connected) {
    DEBUG_LOG("mqtt_send_topic() FAILED, no connection");
    return false; // needs connection
  }
  #ifdef DEBUG_MODE
  char buf_debug[200];
  snprintf(buf_debug, sizeof(buf_debug), "  Topic '%s' = '%s'", topic, value);
  Serial.println(buf_debug);
  #endif
  return g_mqtt_client.publish(topic, value);
}

/* Send MQTT autodiscover topic to home-assistant
 */
bool mqtt_send_autodiscover(WIFI_SETTINGS_T *data) {
  DEBUG_LOG("mqtt_send_autodiscover()");
  char buf_topic[200], buf_value[400];
  char state_topic[100];
  snprintf(state_topic, sizeof(state_topic), "softplus/%s/state", 
    data->mqtt_client_id);

  snprintf(buf_value, sizeof(buf_value), 
    "{\"stat_t\":\"%s\",\"name\":\"%s\",\"off_delay\":30,\"dev\":{"
    "\"name\":\"fastbutton\",\"mdl\":\"%s\",\"ids\":\"%s\"}}",
    state_topic, data->mqtt_client_id, data->mqtt_client_id, data->mqtt_client_id);

  snprintf(buf_topic, sizeof(buf_topic), 
    "homeassistant/binary_sensor/%s/config", data->mqtt_client_id);

  return mqtt_send_topic(buf_topic, buf_value);
}

/* Send MQTT network information topic
 */
bool mqtt_send_network_info(ESP8266WiFiClass *w, WIFI_SETTINGS_T *data) {
  DEBUG_LOG("mqtt_send_network_info()");
  char buf_topic[200], buf_value[200];
  bool result;

  // device ip
  snprintf(buf_topic, sizeof(buf_topic), "softplus/%s/ip", data->mqtt_client_id);
  strcpy(buf_value, w->localIP().toString().c_str());
  result = mqtt_send_topic(buf_topic, buf_value);
  if (!result) return false;

  // device mac
  snprintf(buf_topic, sizeof(buf_topic), "softplus/%s/mac", data->mqtt_client_id);
  strcpy(buf_value, w->macAddress().c_str());
  return mqtt_send_topic(buf_topic, buf_value);
}

/* Send MQTT device state
 */
bool mqtt_send_device_state(WIFI_SETTINGS_T *data) {
  DEBUG_LOG("mqtt_send_device_state()");
  char buf_topic[200], buf_value[200];
  bool result;

  snprintf(buf_topic, sizeof(buf_topic), "softplus/%s/state", data->mqtt_client_id);
  snprintf(buf_value, sizeof(buf_value), "ON");
  result = mqtt_send_topic(buf_topic, buf_value);
  if (!result) return false;

  snprintf(buf_topic, sizeof(buf_topic), "softplus/%s/runtime", data->mqtt_client_id);
  snprintf(buf_value, sizeof(buf_value), "%lu", millis()-g_start_millis);
  return mqtt_send_topic(buf_topic, buf_value);
}
