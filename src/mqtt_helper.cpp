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

/* mqtt_helper.cpp */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "main.h"
#include "settings.h"
#include "wifi_helper.h"

bool g_mqtt_connected;
PubSubClient g_mqtt_client;
extern unsigned long g_start_millis;


/* MQTT  ----------------------------------------------------------- */
/* ----------------------------------------------------------------- */


/* Try to connect to MQTT server, if needed
 */
bool mqtt_connect_server(WiFiClient *wclient, WIFI_SETTINGS_T *data) {
    DEBUG_LOG("mqtt_connect_server()");
    if (g_mqtt_connected) return true;
    if (!data->mqtt_host_ip) {
        DEBUG_LOG("No MQTT IP known");
        return false; // no MQTT hostname
    }

    // Pre-connect to IP address
    #define PRECONNECT_TIMEOUT 5000
    uint32_t timeout = millis() + PRECONNECT_TIMEOUT;
    while ((!wclient->connect(data->mqtt_host_ip, data->mqtt_host_port)) 
            && (millis()<timeout)) { delay(50); }
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
    if (!data->mqtt_homeassistant_topic[0]) return true; 

    char buf_topic[200], buf_value[400];
    char state_topic[100];
    snprintf(state_topic, sizeof(state_topic), "softplus/%s/state", 
    data->mqtt_client_id);

    snprintf(buf_value, sizeof(buf_value), 
        "{\"stat_t\":\"%s\",\"name\":\"%s\",\"off_delay\":30,\"dev\":{"
        "\"name\":\"fastbutton\",\"mdl\":\"%s\",\"ids\":\"%s\"}}",
        state_topic, data->mqtt_client_id, data->mqtt_client_id, data->mqtt_client_id);

    snprintf(buf_topic, sizeof(buf_topic), 
        "%s/binary_sensor/%s/config", 
        data->mqtt_homeassistant_topic, data->mqtt_client_id);

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


/* Disconnect from MQTT server, if connected
 */
void mqtt_disconnect() {
    DEBUG_LOG("mqtt_disconnect()");

    if (g_mqtt_connected) {
        g_mqtt_client.disconnect();
        g_mqtt_connected = false;
    }
}
