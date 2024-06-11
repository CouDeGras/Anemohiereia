#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <Arduino_JSON.h>
#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <mDNS.h>
#include <ArduinoOTA.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

#define DOP 12 // Doppler radar pin
#define IND 2  // Indicator pin

long inactivity_timer = 0;
long offline_timer = 0;
unsigned long lastTime = 0;
unsigned long timerDelay = 3600000; // 1 hour

const char* ssid = "text";
const char* passphrase = "text";
String st;
String content;

String openWeatherMapApiKey = "b7d07ce92538402d1dac83f838c56932";
String city = "MANQUE";
String countryCode = "MANQUE";
bool rebootUpdate = 0;
String weather_actual;
String jsonBuffer;
String timezone = "8";
bool miseajour = 0;
JSONVar myObject;

WebServer server(80);

void startNTPClient();
void setupOTA();
String readStringFromEEPROM(int start, int length);
void getWeather(void *parameter);
void getNTPTime(void *parameter);
String httpGETRequest(const char* serverName);
bool testWifi(void);
void launchWeb(void);
void setupAP(void);
void createWebServer();
void processWeatherData();

String keepOnlyASCII(String input) {
  String output = "";
  for (int i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c >= 0 && c <= 127) {
      output += c;
    }
  }
  return output;
}

void setup() {
  pinMode(DOP, INPUT);
  pinMode(IND, OUTPUT);
  digitalWrite(0, 0);
  digitalWrite(IND, 1);
  delay(700);
  Serial.begin(115200); // Initialize Serial Monitor
  Serial.println();

  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  delay(100);  // Ensure WiFi disconnect is complete
  
  EEPROM.begin(512); // Initialize EEPROM
  delay(10);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);
  
  Serial.println("Startup");
  
  String esid = keepOnlyASCII(readStringFromEEPROM(0, 32));
  String epass = keepOnlyASCII(readStringFromEEPROM(32, 64));
  city = keepOnlyASCII(readStringFromEEPROM(96, 32));
  countryCode = keepOnlyASCII(readStringFromEEPROM(128, 32));
  timezone = keepOnlyASCII(readStringFromEEPROM(160, 32));
  
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.print("PASS: ");
  Serial.println(epass);
  Serial.print("CITY: ");
  Serial.println(city);
  Serial.print("COUNTRY: ");
  Serial.println(countryCode);
  Serial.print("Timezone: ");
  Serial.println(timezone);
  
  WiFi.begin(esid.c_str(), epass.c_str());
  if (testWifi()) {
    Serial.println("SUCCESS");
    Serial.print("Connected to ");
    Serial.println(esid.c_str());
    startNTPClient();
  } else {
    Serial.println("Turning the HotSpot On");
    setupAP(); // Setup HotSpot
  }

  Serial.println("Waiting.");
  while ((WiFi.status() != WL_CONNECTED)) {
    Serial.print(".");
    delay(100);
    server.handleClient();
  }

  // Start FreeRTOS tasks
  xTaskCreate(getWeather, "Weather Task", 4096, NULL, 1, NULL);
  xTaskCreate(getNTPTime, "NTP Task", 2048, NULL, 1, NULL);

  setupOTA();
}

void startNTPClient() {
  timeClient.begin();
  int offset = timezone.toInt() * 3600;
  timeClient.setTimeOffset(offset);
}

void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

String readStringFromEEPROM(int start, int length) {
  String value = "";
  for (int i = 0; i < length; ++i) {
    char c = char(EEPROM.read(start + i));
    // Check if character is printable ASCII and not a null terminator
    if (c >= 32 && c <= 126) {
      value += c;
    }
  }
  value.trim(); // Remove any leading/trailing whitespace
  return value;
}

void getWeather(void *parameter) {
  for (;;) { // FreeRTOS task loop
    if (WiFi.status() == WL_CONNECTED) {
      String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openWeatherMapApiKey;
      Serial.println(serverPath);
      jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(jsonBuffer);
      myObject = JSON.parse(jsonBuffer);
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
      } else {
        // Process weather data
        processWeatherData();
      }
    }
    vTaskDelay(timerDelay / portTICK_PERIOD_MS); // Delay for the set timer period
  }
}

void processWeatherData() {
  // Process JSON data and display weather info
  int tmp = myObject["main"]["temp"];
  tmp -= 273;
  Serial.print("Temperature: ");
  Serial.print(tmp);
  Serial.println(" °C ");
  Serial.print("Humidity: ");
  Serial.println(myObject["main"]["humidity"]);
  // Add more processing as needed
}

void getNTPTime(void *parameter) {
  for (;;) { // FreeRTOS task loop
    if (WiFi.status() == WL_CONNECTED) {
      timeClient.update();
      Serial.print("NTP Time: ");
      Serial.println(timeClient.getFormattedTime());
    }
    vTaskDelay(timerDelay / portTICK_PERIOD_MS);
  }
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  int httpResponseCode = http.GET();
  String payload = "{}";
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    rebootUpdate = 0;
  }
  http.end();
  return payload;
}

void loop() {
  ArduinoOTA.handle();
  unsigned long currentMillis = millis();
  if ((WiFi.status() == WL_CONNECTED)){
    offline_timer = currentMillis;
  } else if (currentMillis - offline_timer > 300000) { // 5 minutes
    ESP.restart(); // auto reboot when disconnected from wifi for more than 5 minutes
  }

  server.handleClient(); // Handle web server client requests
}

bool testWifi(void)
{
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while (c < 20) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("");
  Serial.println("Connection timed out, opening AP");
  return false;
}

void launchWeb()
{
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("WiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}

void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);

    st += ")";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP("Apo_Anemohiereia", "");
  Serial.println("softap");
  launchWeb();
  Serial.println("over");
}

void createWebServer()
{
  {
    server.on("/", []() {
      content = "<!DOCTYPE HTML><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Configuration Interface</title><style>body {font-family: 'Reddit Sans', sans-serif;margin: 0;padding: 0;background-color: #f0f0f0;}#container {max-width: 1024px;margin: 0 auto;background-color: #fff;padding: 20px;box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);}h1, h2, h3, h4, h5, h6 {font-family: 'Marcellus', serif;}.top-bar {background-color: #34495e;color: #fff;padding: 10px 20px;display: flex;justify-content: space-between;align-items: center;}.dropdown-content {display: none;position: absolute;background-color: #f9f9f9;min-width: 160px;box-shadow: 0px 8px 16px 0px rgba(0,0,0,0.2);z-index: 1;}.dropdown-content a {color: black;padding: 12px 16px;text-decoration: none;display: block;}.dropdown-content a:hover {background-color: #f1f1f1;}.dropdown:hover .dropdown-content {display: block;}input[type=text] {width: 100%;padding: 12px 20px;margin: 8px 0;box-sizing: border-box;border: 2px solid #ccc;border-radius: 4px;background-color: #f8f8f8;}input[type=submit] {background-color: #4CAF50;color: white;padding: 14px 20px;margin: 8px 0;border: none;border-radius: 4px;cursor: pointer;}input[type=submit]:hover {background-color: #45a049;}input::placeholder {color: gray;font-style: italic;}</style></head><body><div id=\"container\"><div class=\"top-bar\"><h1>Configuration Interface</h1><div class=\"dropdown\"><button class=\"dropbtn\">Menu</button><div class=\"dropdown-content\"><a href=\"#\">Home</a><a href=\"#\">Settings</a><a href=\"#\">Help</a></div></div></div><form method='get' action='setting'><label>WIFI SSID:</label><input name='ssid' type=\"text\" length=32 placeholder=\"Enter SSID\"><br><label>Password:</label><input name='pass' type=\"text\" length=64 placeholder=\"Enter password\"><br><label>City name:</label><input name='city' type=\"text\" length=32 placeholder=\"(Shanghai)\"><br><label>Country code:</label><input name='country' type=\"text\" length=32 placeholder=\"(CN)\"><br><label>Timezone:</label><input name='timezone' type=\"text\" length=32 placeholder=\"(8)\"><br><input type='submit' value='Save Configuration'></form></div></body></html>";
      server.send(200, "text/html", content);
    });
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });

    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      String qcity = server.arg("city");
      String qcountry = server.arg("country");
      String qtimezone = server.arg("timezone");
        int statusCode = 0;
      if (qsid.length() > 0 && qpass.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 256; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");

        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        } for (int i = 0; i < qcity.length(); ++i)
        {
          EEPROM.write(96 + i, qcity[i]);
          Serial.print("Wrote: ");
          Serial.println(qcity[i]);
        } for (int i = 0; i < qcountry.length(); ++i)
        {
          EEPROM.write(128 + i, qcountry[i]);
          Serial.print("Wrote: ");
          Serial.println(qcountry[i]);
        } for (int i = 0; i < qtimezone.length(); ++i)
        {
          EEPROM.write(160 + i, qtimezone[i]);
          Serial.print("Wrote: ");
          Serial.println(qtimezone[i]);
        }
        EEPROM.commit();

        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.restart();
      }
      else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);

    });
  }
}
