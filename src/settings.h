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

/* settings.h */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <ESP8266WiFi.h>

/* Our data structure for WIFI settings */
#define SETTINGS_MAGIC_NUM 0x1AC4
#define SETTINGS_VERSION 2

struct WIFI_SETTINGS_T { // size: 644 bytes
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
  uint8_t version;
  char filler[380]; // not used
};

void save_settings_to_flash(WIFI_SETTINGS_T *data);
bool get_settings_from_flash(WIFI_SETTINGS_T *data);
void default_settings(WIFI_SETTINGS_T *data);
void build_settings_from_wifi(WIFI_SETTINGS_T *data, ESP8266WiFiClass *w);
void set_settings_ap(WIFI_SETTINGS_T *data, char *ssid, char *auth);
void show_settings(WIFI_SETTINGS_T *data);

#endif
