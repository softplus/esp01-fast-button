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

/* fast_wifi.cpp */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "main.h"
#include "settings.h"
#include "wifi_helper.h"

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

  //set_settings_ap(data, (char *)WIFI_SSID, (char *)WIFI_AUTH);
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


/* Show information about the global WiFi object, if we're in debug-mode
 */
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
