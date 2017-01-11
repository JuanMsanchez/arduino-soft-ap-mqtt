ESP8266 IOT PROJECT
===================
Early stage version, with mqtt client, soft AP mode, custom parameters for mqtt user and password and FS persistence.
This project subscribe temperature and relative humidity data to an authenticated mqtt broker in json format.
Also counts with a pushbutton to set the nodemcu in AP mode for easy ssid, password and mqtt credentials configuration.
The configuration provided in the AP config form will be stored in the esp8266 file system.

* The AP network name is esp-ap-config and the password admin

###Libraries used by this project
- Arduino core for ESP8266 - https://github.com/esp8266/Arduino (not really a library)
- WiFiManager - https://github.com/tzapu/WiFiManager
- ArduinoJson - https://github.com/bblanchon/ArduinoJson
- PubSubClient - https://github.com/knolleary/pubsubclient
- DHT - https://github.com/adafruit/DHT-sensor-library

###Hardware:
- nodemcu devkit v1.0
- DHT11
- 1k resistor
- pushbutton

###Breadboard configuration
![alt tag](https://raw.githubusercontent.com/JuanMsanchez/arduino-soft-ap-mqtt/master/breadboard.jpg)
