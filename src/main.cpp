/*
  Copyright (c) 2022-2022 John Mueller

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* main.cpp */

/* Main function loops. */

#include <Arduino.h>

#include "main.h"
#include "settings.h"
#include "wifi_helper.h"
#include "mqtt_helper.h"

#define NOTIFY_PIN 3
#define LED_PIN 2

WIFI_SETTINGS_T g_wifi_settings;
bool g_wifi_mqtt_working;
unsigned long g_start_millis; // millis() counter at start
WiFiClient g_wclient;

// functions that follow
void countdown(int secs);


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
  bool autodiscover_mqtt = false;

  DEBUG_LOG("\n## WIFI:");
  if (!get_settings_from_flash(&g_wifi_settings)) {
    default_settings(&g_wifi_settings);
    // todo: this will fail without credentials
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
  mqtt_disconnect();
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
  DEBUG_LOG("... FAILED ... you should never be here, let's try reset");
  ESP.reset();
  DEBUG_LOG("... FAILED ... let's sleep");
  ESP.deepSleep(30e6); 
  #endif
  // never does second loop
}


/* Show some debugging information --------------------------------- */
/* ----------------------------------------------------------------- */

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
