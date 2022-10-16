# esp01-fast-button

Code sample for a basic ESP-01 Wifi-button, showing how to cache the wifi connection data to achieve a faster connection to the access point.
Regularly connects to wifi and sends MQTT requests within 1200ms.

[MIT license](LICENSE) / (c) 2022 [John Mueller](https://johnmu.com/)

# Hardware

Sample hardware uses an ESP-01 module, with pin 3 being used to control the power to the module. The hardware could be simplified to go to deep sleep, but I like the idea of pulling out your own power cord :).

## High-level hardware flow

1. Button turns power on briefly (needs ca 100ms at least)
2. ESP-01 wakes up, pulls power line high to keep power on
3. ESP-01 does a fast connect to the wifi AP
4. ESP-01 sends MQTT packet as desired
5. ESP-01 turns power line low & goes offline

# Wifi fast connection

Part of what makes the wifi connection to the AP slow is getting the BSSID of the AP, getting the right channel, getting a local IP address, getting the DNS information, and getting the IP of the MQTT server. We can cache all of these and reduce the timing significantly. This is particularly possible if the device we're using is in a stable location (same AP, channel), and the AP being connected to is mostly static (same channel, local IPs).

## High-level flow

1. Check if we have a cache available
    1. If so:
        1. try to connect to the last-known BSSID
        2. try to use the last-provided device IP, DNS
        3. pre-connect to MQTT using the IP address, port
        4. send MQTT packets
    2. If not:
       1. Connect using traditional methods
       2. Get MQTT server's IP address
       3. save cached connection information (BSSID, IPs, etc)
       4. connect & send MQTT packets
    3. If no valid settings:
       1. Jump to AP mode below

2. Blink a bit and pull own power plug
3. If someone's still pushing the button, refresh the wifi connection cache
4. Wait 4 seconds
5. If someone's still pushing the button, start AP mode (blink at 0.5Hz). 
6. Remain in AP mode 5 minutes, await connection
7. If connection: remain in AP mode for 5 minutes

# Access point for configuration

Implements a captive portal so you just need to connect to the AP with your (Android) phone and it'll take you to the home page directly.
For direct access, use http://192.168.4.1/ .

The name of the access point depends on the device's MAC address, it looks something like "AP_AA1122".

The homepage of the access point allows configuration of wifi name, authentication, MQTT server settings, and MQTT request to send upon click.
It does not check the wifi settings, but if they're wrong, it'll revert to the AP mode again.

# To-do's

* add hardware schematic, circuit board
* confirm timings of individual steps
* double-check char-array sizes

# FYI

* OTA not possible on 500MB devices: Flash usage > 250MB
