# Anemohiereia
OpenWeatherMap-enabled ESP8266 weather display

**Hardware:**

1. ESP8266 Dev board

2. 1602 LCD

3. An I2C Converter for 1602 LCD

**Steps:**

1. Register an account at Openweathermap.org, obtain an API key (free plan will suffice)

2. Upload the code to your ESP8266

3. A wifi hotspot should appear as Apo_Anemohiereia. Access 192.168.4.1 to configure your ESP8266

4. SSID (Your household wifi name); PASS (Your wifi password); CITY (Uppercase first letter); COUNTRY (Two-letter-code); TIMEZONE(integer hour);

Input format example:

myWifi, myPasswd, Liverpool, GB, 0
dormWifi, dormPasswd, Shanghai, CN, 8
