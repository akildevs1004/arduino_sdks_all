// #define ARDUHAL_LOG_LEVEL 0  // Disable WiFi/HTTP debug logs
//Model ESP32-WROOM-DA Module
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include "PCF8574.h"
#include <HTTPClient.h>
// #include <ArduinoJson.h>
#include <WiFiClient.h>
// #include <Update.h>
#include <ArduinoOTA.h>
#include <ModbusMaster.h>

#define DEBUG 0  // Set to 1 to enable debug
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
int cloudAccountActiveDaysRemaining = 100;
unsigned long lastRun = 0;
const unsigned long interval = 24UL * 60UL * 60UL * 1000UL;  // 24 hours in milliseconds
String serverURL = "";
String todayDate;
String device_serial_number = "XT123456";
bool USE_ETHERNET = true;
bool USE_DEFAULT_WIFIMANGER = false;
String firmWareVersion = "2.0";

void setup() {
  Serial.begin(115200);



  Serial.printf("Flash size (bytes): %u\n", ESP.getFlashChipSize());


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


    DeviceSetup();
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

    // configTime(0, 0, "pool.ntp.org");
    // delay(2000);  // Wait for NTP sync

    // // // Get today's date
    // todayDate = getCurrentDate();
    // Serial.println("Today's Date: " + todayDate);


    socketConnectServer();
    handleHeartbeat();
    //getDeviceAccoutnDetails();

    updateFirmWaresetup();
    uploadHTMLsetup();
    cloudAccountActiveDaysRemaining = 100;
    if (cloudAccountActiveDaysRemaining <= 0) {
      Serial.println("❌ XXXXXXXXXXXXXXXXXXXXXXXXXXXXX----Account is expired----XXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    }
  }
}

void loop() {

  Deviceloop();



  server.handleClient();

  if (WiFi.status() == WL_CONNECTED || USE_ETHERNET) {

    // if (cloudAccountActiveDaysRemaining > 0) {

    //   handleHeartbeat();
    //   updateFirmWareLoop();

    //   unsigned long currentMillis = millis();


    //   if (currentMillis - lastRun >= interval) {
    //     lastRun = currentMillis;
    //     // getDeviceAccoutnDetails();
    //   }
    // }

    // deviceReadSensorsLoop();
    delay(200);  // Non-blocking delay
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
