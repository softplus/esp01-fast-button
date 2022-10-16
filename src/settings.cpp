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

/* settings.cpp */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include "main.h"
#include "settings.h"

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
    Serial.print(F("  Settings size: ")); Serial.println(sizeof(buf));
    Serial.print(F("  Peek: "));
    char *d = (char *)data;
    for (int i=0; i<16; i++) {
        snprintf(b,sizeof(b), "%02x ", *d++); Serial.print(b);
    }
    Serial.println();
    #endif

    if ((data->magic==SETTINGS_MAGIC_NUM) && (data->version<SETTINGS_VERSION)) {
        // upgrade settings
        DEBUG_LOG("Upgrading settings structure");
        data->version=SETTINGS_VERSION;
        save_settings_to_flash(data);
    }

    return (data->magic == SETTINGS_MAGIC_NUM);
}


/* Creates default settings structure
 */
void default_settings(WIFI_SETTINGS_T *data) {
    DEBUG_LOG("default_settings()");

    // main settings
    memset(data, 0, sizeof(*data));
    data->magic = SETTINGS_MAGIC_NUM;
    strncpy(data->mqtt_host_str, "homeassistant.local", sizeof(data->mqtt_host_str));
    data->mqtt_host_port = 1883;
    strncpy(data->mqtt_user, "username", sizeof(data->mqtt_user));
    strncpy(data->mqtt_auth, "password", sizeof(data->mqtt_auth));
    strncpy(data->mqtt_client_id, "FASTBUTTON", sizeof(data->mqtt_client_id));
    strncpy(data->mqtt_topic, "wled/lights", sizeof(data->mqtt_topic));
    strncpy(data->mqtt_value, "T", sizeof(data->mqtt_value));
    data->version = SETTINGS_VERSION;
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
    memcpy(data->wifi_bssid, w->BSSID(), 6);
    data->wifi_channel = w->channel();
    // look up IP for MQTT server
    if (data->mqtt_host_str[0]) {
        IPAddress mqtt_ip;
        int err = w->hostByName(data->mqtt_host_str, mqtt_ip);
        if (err==0) { 
            data->mqtt_host_ip = 0;
            #ifdef DEBUG_MODE
            Serial.print(" ** Can't resolve host: "); Serial.println(data->mqtt_host_str); 
            #endif
        } else {
            data->mqtt_host_ip = (uint32_t)mqtt_ip;
        }
    } else {
        data->mqtt_host_ip = 0;
    }
}


/* Save AP SSID & AUTH to data structure */
void set_settings_ap(WIFI_SETTINGS_T *data, char *ssid, char *auth) {
    DEBUG_LOG("wifi_set_ap()");

    strncpy(data->wifi_ssid, ssid, sizeof(data->wifi_ssid));
    data->wifi_ssid[sizeof(data->wifi_ssid)-1] = 0;
    strncpy(data->wifi_auth, auth, sizeof(data->wifi_auth));
    data->wifi_auth[sizeof(data->wifi_auth)-1] = 0;
}


/* If we're debugging, show the full settings
 */
void show_settings(WIFI_SETTINGS_T *data) {
    DEBUG_LOG("show_settings()");

    #ifdef DEBUG_MODE
    char buf[200];
    Serial.println("Settings:");
    IPAddress ip;
    snprintf(buf, sizeof(buf), "Magic:        %04X", data->magic); Serial.println(buf);
    ip = data->ip_address;
    snprintf(buf, sizeof(buf), "Local IP:     %s", ip.toString().c_str()); Serial.println(buf);
    ip = data->ip_gateway;
    snprintf(buf, sizeof(buf), "Gateway IP:   %s", ip.toString().c_str()); Serial.println(buf);
    ip = data->ip_mask;
    snprintf(buf, sizeof(buf), "Mask:         %s", ip.toString().c_str()); Serial.println(buf);
    ip = data->ip_dns1;
    snprintf(buf, sizeof(buf), "DNS 1 IP:     %s", ip.toString().c_str()); Serial.println(buf);
    ip = data->ip_dns2;
    snprintf(buf, sizeof(buf), "DNS 2 IP:     %s", ip.toString().c_str()); Serial.println(buf);
    snprintf(buf, sizeof(buf), "Wifi SSID:    %s", data->wifi_ssid); Serial.println(buf);
    snprintf(buf, sizeof(buf), "Wifi Auth:    %s", data->wifi_auth); Serial.println(buf);
    snprintf(buf, sizeof(buf), "Wifi BSSID:   "
        "%02X:%02X:%02X:%02X:%02X:%02X", 
        data->wifi_bssid[0], data->wifi_bssid[1], data->wifi_bssid[2], 
        data->wifi_bssid[3], data->wifi_bssid[4], data->wifi_bssid[5]); Serial.println(buf);
    snprintf(buf, sizeof(buf), "Wifi Channel: %d", data->wifi_channel); Serial.println(buf);
    snprintf(buf, sizeof(buf), "MQTT Host:    %s", data->mqtt_host_str); Serial.println(buf);
    ip = data->mqtt_host_ip;
    snprintf(buf, sizeof(buf), "MQTT IP:      %s", ip.toString().c_str()); Serial.println(buf);
    snprintf(buf, sizeof(buf), "MQTT Port:    %d", data->mqtt_host_port); Serial.println(buf);
    snprintf(buf, sizeof(buf), "MQTT User:    %s", data->mqtt_user); Serial.println(buf);
    snprintf(buf, sizeof(buf), "MQTT Pass:    %s", data->mqtt_auth); Serial.println(buf);
    snprintf(buf, sizeof(buf), "MQTT ClientID:%s", data->mqtt_client_id); Serial.println(buf);
    snprintf(buf, sizeof(buf), "MQTT Topic:   %s", data->mqtt_topic); Serial.println(buf);
    snprintf(buf, sizeof(buf), "MQTT Value:   %s", data->mqtt_value); Serial.println(buf);
    #endif
}
