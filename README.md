# Anemohiereia
OpenWeatherMap-enabled ESP8266 weather display

**Hardware:**

1. ESP8266 Dev board

2. 0802/1602 LCD

3. An I2C Converter for 0802/1602 LCD

**Steps:**

1. Register an account at Openweathermap.org, obtain an API key (free plan will suffice). Swap your key to the code.

2. Upload the code to your ESP8266

3. A wifi hotspot should appear as Apo_Anemohiereia. Access 192.168.4.1 to configure your ESP8266

4. SSID (Your household wifi name); PASS (Your wifi password); CITY (Uppercase first letter); COUNTRY (Two-letter-code); TIMEZONE(integer hour);

Input format example:

myWifi, myPasswd, Liverpool, GB, 0

dormWifi, dormPasswd, Shanghai, CN, 8

5. Submit and 8266 should reboot, ideally the weather would appear on screen.

To make the configuration hotspot appear again, prevent your 8266 from connecting to the last memorized network. (e.g. distance your 8266 from router, or unplug your router, or open a phone hotspot w/ same name as your home router but w/ different password). 
