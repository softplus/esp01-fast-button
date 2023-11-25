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
#include "ap_mode.h"
#include <ESP8266HTTPClient.h>

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
	WiFi.persistent(true);

	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, LOW); // LED on

	#ifdef DEBUG_MODE
	Serial.begin(115200);
	Serial.print("Starting soon...");
	countdown(4);
	uint32_t finish_wifi_millis = 0;
	#endif
	g_start_millis = millis();

	g_wifi_mqtt_working = false; // assume the worst
	bool autodiscover_mqtt = false;

	DEBUG_LOG("\n## WIFI:");
	if (!get_settings_from_flash(&g_wifi_settings)) {
		// if we have no settings, start with default
		default_settings(&g_wifi_settings);
		g_wifi_mqtt_working = false;
	} else {
		// connect to wifi
		#ifdef DEBUG_MODE
		show_settings(&g_wifi_settings);
		#endif
		// attempt fast connect first
		g_wifi_mqtt_working = wifi_try_fast_connect(&g_wifi_settings, &WiFi);
		if (!g_wifi_mqtt_working) {
			// traditional wifi connection
			g_wifi_mqtt_working = wifi_try_slow_connect(&g_wifi_settings, &WiFi);
			if (g_wifi_mqtt_working) save_settings_to_flash(&g_wifi_settings);
			autodiscover_mqtt = true;
		}
	}
	#ifdef DEBUG_AUTODISCOVER
	autodiscover_mqtt = true;
	#endif
	#ifdef DEBUG_MODE
	finish_wifi_millis = millis();
	#endif
	
	DEBUG_LOG("\n## MQTT:");
	if (g_wifi_mqtt_working) {
		#ifdef DEBUG_MODE
		show_wifi_info(&WiFi);
		#endif
		#ifndef DEBUG_SKIP_MQTT
		// check if we have a MQTT hostname
		if (g_wifi_settings.mqtt_host_str[0]) {
			if (!mqtt_connect_server(&g_wclient, &g_wifi_settings)) {
				DEBUG_LOG("mqtt_connect_server() FAILED");
				g_wifi_mqtt_working = false;
			}
			if (g_wifi_mqtt_working) {
				if (!mqtt_send_topic(g_wifi_settings.mqtt_topic, g_wifi_settings.mqtt_value)) {
					DEBUG_LOG("mqtt_send_topic(main) FAILED");
					g_wifi_mqtt_working = false;
				}
			}
			if (g_wifi_mqtt_working) {
				if (autodiscover_mqtt) {
					mqtt_send_autodiscover(&g_wifi_settings);
					mqtt_send_network_info(&WiFi, &g_wifi_settings);
				}
				mqtt_send_device_state(&g_wifi_settings);
			}
		}
		#endif
		#ifndef DEBUG_SKIP_REST
			// check if we have a REST URL
			if (g_wifi_settings.rest_url[0]) {
				WiFiClient client;
				HTTPClient http;
				DEBUG_LOG("Requesting REST URL: ");
				Serial.println(g_wifi_settings.rest_url);

				http.begin(client, g_wifi_settings.rest_url);
		    	// Send HTTP GET request
      			int http_response_code = http.GET();
				Serial.println(http_response_code);
				// ignore response code, we're done 
			}
		#endif

	}
	#ifdef DEBUG_MODE
	Serial.print("Result: ");
	if (g_wifi_mqtt_working) Serial.println("OK"); else Serial.println("FAILED");

	Serial.println();
	Serial.print("Time Wifi: ");
	Serial.print((finish_wifi_millis-g_start_millis));
	Serial.println(" ms");
	Serial.print("Time total: ");
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

	#ifdef DEBUG_AP_MODE
	bool res = enable_ap_mode(&g_wifi_settings);
	if (res) run_ap_mode(&g_wifi_settings); // reboots afterwards
	#endif

	#ifndef DEBUG_MODE
	if (g_wifi_mqtt_working) {
		// @ ca 3s
		for (int i=0; i<1500/100; i++) {
			digitalWrite(LED_PIN, ((i%2)==0)?LOW:HIGH); delay(100);
		}
		// @ ca 5s
		digitalWrite(LED_PIN, HIGH); // LED off
		digitalWrite(NOTIFY_PIN, LOW); // should power down
		delay(2000); // wait a little; @ ca 7s
		// if still here, rebuild it all
		digitalWrite(LED_PIN, LOW); // LED on
		digitalWrite(NOTIFY_PIN, HIGH); // keep power up now
		delay(100);
		// disconnect MQTT, reconnect Wifi, cache state, do autodiscovery
		mqtt_disconnect();
		bool res = wifi_try_slow_connect(&g_wifi_settings, &WiFi);
		if (res) {
			save_settings_to_flash(&g_wifi_settings);
			if (mqtt_connect_server(&g_wclient, &g_wifi_settings)) {
				mqtt_send_network_info(&WiFi, &g_wifi_settings);
				mqtt_send_autodiscover(&g_wifi_settings);
			}
		}
		// complete, @ ca 12s
		digitalWrite(LED_PIN, HIGH); // LED off
		delay(200); // wait a little
		digitalWrite(NOTIFY_PIN, LOW); // power down again
	}
	// is anyone still pushing the button? start AP mode.
	// @ ca 15s, or 3s after first start
	digitalWrite(NOTIFY_PIN, HIGH); // power up
	bool res = enable_ap_mode(&g_wifi_settings);
	if (res) run_ap_mode(&g_wifi_settings);
	// reboots afterwards
	#endif

	#ifdef DEBUG_MODE
	countdown(4);
	#endif

	DEBUG_LOG("ESP.restart() ...");
	ESP.restart(); ESP.reset();
	DEBUG_LOG("... Restart & reset failed ... let's sleep");
	ESP.deepSleep(30e6);
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
