# esp01-fast-button

Code sample for a basic ESP-01 Wifi-button, showing how to cache the wifi connection data to achieve a faster connection to the access point. Regularly connects within 200ms.

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
       1. Jump to OTA mode below

2. Blink a bit and pull own power plug
3. If someone's still pushing the button, refresh the wifi connection cache
4. Wait 4 seconds
5. If someone's still pushing the button, start OTA mode
6. Remain in OTA mode 5 minutes, await connection
7. If connection: remain in OTA mode for 5 minutes

# To-do's

* add hardware schematic, circuit board
* confirm timings of individual steps
* add Soft-AP mode for making settings
  * trigger AP mode properly
  * display last IP address in AP mode
* escape MQTT JSON strings properly
* escape MQTT topic, value

# FYI

* OTA not possible: Flash usage > 50% of 500MB storage
