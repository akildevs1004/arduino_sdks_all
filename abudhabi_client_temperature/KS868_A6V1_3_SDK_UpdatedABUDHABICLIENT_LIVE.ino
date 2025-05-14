#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <DHT22.h>
#include <HTTPClient.h> 
#include <Update.h>
#include <WiFiClient.h>
 
 
#include <time.h>




WiFiManager wifiManager;
DynamicJsonDocument config(1024);  // Allocate 1024 bytes for JSON storage
String sessionToken = "";

bool loginStatus = false;

WiFiClient client;  // Create a client object
WebServer server(80);
String deviceConfigContent;
String sensorData;
String DeviceIPNumber;
String loginErrorMessage;
String GlobalWebsiteResponseMessage;
String GlobalWebsiteErrorMessage;
HTTPClient http;
int cloudAccountActiveDaysRemaining = 90;
unsigned long lastRun = 0;
const unsigned long interval = 24UL * 60UL * 60UL * 1000UL;  // 24 hours in milliseconds 
String serverURL = "";   
String todayDate;
String device_serial_number = "24000003";
bool USE_ETHERNET = false;
bool USE_DEFAULT_WIFIMANGER = true;
String firmWareVersion = "2.0";

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS is Not available");

    delay(1000);
  } else {
    String savedData = readConfig("config.json");
    Serial.println(savedData);
    if (savedData != "") {
      deserializeJson(config, savedData);
      if (config["wifi_or_ethernet"].as<String>() == "1")
        USE_ETHERNET = false;
    }
  }
  if (USE_DEFAULT_WIFIMANGER) {

    connectDefaultWifiAuto();

  } else {
    startNetworkProcessStep1();  //load config and start Device web server
  }

  lastRun = millis();  // Initial timestamp

  routes();  // Define Routes and handlers
  server.begin();
  Serial.println("HTTP Server started");


  if (WiFi.status() == WL_CONNECTED) {

    delay(1000);
    updateJsonConfig("config.json", "ipaddress", DeviceIPNumber);
    updateJsonConfig("config.json", "firmWareVersion", firmWareVersion);

    configTime(0, 0, "pool.ntp.org");
    delay(2000);  // Wait for NTP sync

    // // Get today's date
    //todayDate = getCurrentDate();
    Serial.println("Today's Date: " + todayDate);


    socketConnectServer();
    handleHeartbeat();
    //getDeviceAccoutnDetails();
    devicePinDefinationSetup();
    //updateFirmWaresetup();
    //uploadHTMLsetup();

    if (cloudAccountActiveDaysRemaining <= 0) {
      Serial.println("❌ XXXXXXXXXXXXXXXXXXXXXXXXXXXXX----Account is expired----XXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    }
  }
}

void loop() {

  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {

    if (cloudAccountActiveDaysRemaining > 0) {



      handleHeartbeat();
      //updateFirmWareLoop();

      unsigned long currentMillis = millis();
     

      if (currentMillis - lastRun >= interval) {
        lastRun = currentMillis;
        //getDeviceAccoutnDetails();
      }
    } else
    {
    }

     deviceReadSensorsLoop();
    delay(1000);  // Non-blocking delay
  }
}

String replaceHeaderContent(String html) {
  html.replace("{firmWareVersion}", firmWareVersion);
  html.replace("{ipAddress}", DeviceIPNumber);
  html.replace("{loginErrorMessage}", loginErrorMessage);
  html.replace("{GlobalWebsiteResponseMessage}", GlobalWebsiteResponseMessage);
  html.replace("{GlobalWebsiteErrorMessage}", GlobalWebsiteErrorMessage);

  html.replace("{cloud_company_name}", config["cloud_company_name"].as<String>());
  html.replace("{cloud_account_expire}", config["cloud_account_expire"].as<String>());
  html.replace("{cloudAccountActiveDaysRemaining}", String(cloudAccountActiveDaysRemaining));

  return html;
}
