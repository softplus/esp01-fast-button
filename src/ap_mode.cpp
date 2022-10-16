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

/* ap_mode.cpp - for a local access point to make settings changes */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "ap_mode.h"
#include "main.h"
#include "settings.h"
#include "wifi_helper.h"

#define AP_TIMEOUT_SECS 5*60
static ESP8266WebServer local_server(80);
static uint32_t ap_timeout;
static uint32_t led_time_next;
static bool led_status;


void _handle_root();
void _handle_404();
void _handle_form();
static WIFI_SETTINGS_T *_data; // pointer to actual data

/* Enables AP mode, if doable
 */
bool enable_ap_mode(WIFI_SETTINGS_T *data) {
	DEBUG_LOG("enable_ap_mode()");

	char ap_name[30]; // AP name is "AP_001122" with 001122 being last MAC digits
	uint8_t mac[6];
	(void)WiFi.macAddress(&mac[0]);
	snprintf(ap_name, sizeof(ap_name), "AP_%02X%02X%02X", mac[3], mac[4], mac[5]);

	bool res = WiFi.softAP(ap_name, ""); // no password
	if (res) {
		DEBUG_LOG("AP mode enabled.");
		#ifdef DEBUG_MODE
		Serial.print("AP: "); Serial.println(ap_name);
		#endif
	}
	return res;
}

/* Blink LED at 1x / 2sec
 */
static void _handle_led() {
	if (millis() > led_time_next) {
		led_time_next = millis() + (led_status?500:1500);
		led_status = !led_status;
		digitalWrite(LED_PIN, led_status?LOW:HIGH);
		#ifdef DEBUG_MODE
		if (led_status) Serial.print(".");
		#endif
	}
}

/* Runs device in AP mode to do settings and stuff
 * Time out after given time, then reboot.
 */
void run_ap_mode(WIFI_SETTINGS_T *data) {
	DEBUG_LOG("run_ap_mode()");
	ap_timeout = millis() + AP_TIMEOUT_SECS * 1000L;
	led_status = false;
	led_time_next = millis();
	_data = data;

	local_server.on("/", _handle_root);
	local_server.on("/get", _handle_form);
	local_server.onNotFound(_handle_404);
	local_server.begin();
	while (millis() < ap_timeout) {
		_handle_led();
		local_server.handleClient();
		delay(50);
	}
	DEBUG_LOG("Rebooting after timeout.");
	delay(500);
	ESP.restart(); ESP.reset();
}

/* show string in HTML escaped form; ignore entities, escape all non-alphanum
 */
void _show_escape_html(char *input) {
	char buf[100];
	char *in_ptr = input;
	char *out_ptr = (char *)&buf;
	while (*in_ptr) {
		if (isalnum(*in_ptr)) {
			*out_ptr=*in_ptr; out_ptr++;
		} else {
			out_ptr += sprintf(out_ptr, "&#%i;", *in_ptr);
		}

		if (out_ptr-(char *)&buf>(int)sizeof(buf)-8) {
			*out_ptr=0;
			local_server.sendContent(buf);
			out_ptr=(char *)&buf;
		}
		in_ptr++;
	}
	*out_ptr=0;
	local_server.sendContent(buf);
}

/* Show one of the fields as a form input
 */
void _show_field(char const *label, char const *id, char *value) {
	local_server.sendContent("<p>");
	local_server.sendContent(label);
	local_server.sendContent(":<br>");
	local_server.sendContent("<input type=\"text\" name=\"");
	local_server.sendContent(id);
	local_server.sendContent("\" value=\"");
	_show_escape_html(value);
	local_server.sendContent("\"></p>\n");
}

/* Serve homepage, with form for settings
 */
void _handle_root() {
	DEBUG_LOG("_handle_root()");

	// header
	local_server.sendContent("HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Access-Control-Allow-Headers: Origin, X-Requested-With, Content-Type, Accept\r\n"           // TEXT MIME type
		"\r\n");

	// page start
	local_server.sendContent( R"rawliteral(<!DOCTYPE HTML><html><head>
		<meta charset="utf-8"><title>Fast Button Setup</title>
		<meta name="robots" content="none">
		<meta name="viewport" content="width=device-width, initial-scale=1">
		</head><body><h1>Fast button setup</h1>
		<form action="/get">)rawliteral" );

	char buf[20];

	_show_field("Wifi SSID", "wifi_ssid", _data->wifi_ssid);
	_show_field("Wifi Password", "wifi_auth", _data->wifi_auth);

	_show_field("MQTT Host", "mqtt_host_str", _data->mqtt_host_str);
	snprintf(buf, sizeof(buf), "%i", _data->mqtt_host_port);
	_show_field("MQTT Port", "mqtt_host_port", buf);
	_show_field("MQTT Username", "mqtt_user", _data->mqtt_user);
	_show_field("MQTT Password", "mqtt_auth", _data->mqtt_auth);

	_show_field("MQTT Client ID", "mqtt_client_id", _data->mqtt_client_id);
	_show_field("MQTT Topic", "mqtt_topic", _data->mqtt_topic);
	_show_field("MQTT Topic value", "mqtt_value", _data->mqtt_value);
	_show_field("MQTT Home Assistant Topic", "mqtt_ha", _data->mqtt_homeassistant_topic);

	// below form
	local_server.sendContent( R"rawliteral(
		<input type="submit" name="submit" value="Save settings">)rawliteral" );
	local_server.sendContent( R"rawliteral(
		<input type="submit" name="reboot" value="Save and reboot">)rawliteral" );
	local_server.sendContent("</form>");

	// page footer start
	local_server.sendContent( R"rawliteral(
		<footer>
		<p>(c) <a href="https://johnmu.com/">johnmu</a> -
		)rawliteral" );

	local_server.sendContent("Built ");
	local_server.sendContent(__DATE__);
	local_server.sendContent(" ");
	local_server.sendContent(__TIME__);
	local_server.sendContent("</p>");

	local_server.sendContent( R"rawliteral(<p>
		This module reboots in <span id="timer">...</span>.</p>
		  <script>
			var finished = Date.now() + )rawliteral" );
	sprintf(buf, "%lu", ap_timeout-millis());
	local_server.sendContent(buf);
	local_server.sendContent( R"rawliteral(;
			setInterval(function() {
			  let remaining = (finished - Date.now())/1000;
			  remaining = (remaining<0)?0:remaining;
			  let mins = (remaining/60)|0;
			  let secs = (remaining%60)|0;
			  document.getElementById("timer").innerHTML = (
				mins + ":" + ((secs<10)?"0":"") + secs);
			}, 1000);
		  </script>)rawliteral" );

	// page footer finish
	local_server.sendContent( R"rawliteral(
		</footer>
		</body></html>)rawliteral" );
}


/* Check if the user submitted the variable, then read it
 * Check for maximal length, store everything. Return 1 if changed.
 */
int _read_field(char const *id, char *dest, int len) {
	if (local_server.hasArg(id)) {
		if (local_server.arg(id).length()<(unsigned int)len-1) {
			int changed = (strcmp(dest, local_server.arg(id).c_str())!=0)?1:0;
			strcpy(dest, local_server.arg(id).c_str());
			return changed;
		}
	}
	return 0;
}


/* Handle submitted form, extract variables & save
 */
void _handle_form() {

	// handle fields
	int changes = 0;
	changes += _read_field("wifi_ssid", _data->wifi_ssid, sizeof(_data->wifi_ssid));
	changes += _read_field("wifi_auth", _data->wifi_auth, sizeof(_data->wifi_auth));

	changes += _read_field("mqtt_host_str", _data->mqtt_host_str, sizeof(_data->mqtt_host_str));
	char buf[20], *end;
	snprintf(buf, sizeof(buf), "%i", _data->mqtt_host_port);
	changes += _read_field("mqtt_port", (char *)&buf, sizeof(buf));
	long int res = strtol(buf, &end, 10);
	if (res>0 && res<32000) _data->mqtt_host_port= (int)res;

	changes += _read_field("mqtt_user", _data->mqtt_user, sizeof(_data->mqtt_user));
	changes += _read_field("mqtt_auth", _data->mqtt_auth, sizeof(_data->mqtt_auth));

	changes += _read_field("mqtt_client_id", _data->mqtt_client_id, sizeof(_data->mqtt_client_id));
	changes += _read_field("mqtt_topic", _data->mqtt_topic, sizeof(_data->mqtt_topic));
	changes += _read_field("mqtt_value", _data->mqtt_value, sizeof(_data->mqtt_value));
	changes += _read_field("mqtt_ha", _data->mqtt_homeassistant_topic, sizeof(_data->mqtt_homeassistant_topic));

	if (changes) {
		// save to flash
		DEBUG_LOG("Found changes, saving to flash.");
		_data->wifi_channel = 0; // forces traditional wifi connect next
		save_settings_to_flash(_data);
	}

	if (local_server.hasArg("reboot")) {
		DEBUG_LOG("Rebooting.");
		// show reboot message
		local_server.sendContent("HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n\r\n");

		local_server.sendContent(
			R"rawliteral(<!DOCTYPE HTML><html><head><meta charset="utf-8" />
			<title>Rebooting</title>
			<meta name="viewport" content="width=device-width, initial-scale=1" />
			</head><body><h1>Rebooting ...</h1><script>history.pushState({},"","/");</script>
			</body></html>)rawliteral" );

		delay(500);
		ESP.restart(); ESP.reset();
	}

	// return to homepage
	local_server.sendContent("HTTP/1.1 302 Temporary redirect\r\n"
		"Location: /\r\n\r\n");
}


/* Handle the 404 page
 */
void _handle_404() {
	local_server.send(404, "text/html", "404 Not found");
}
