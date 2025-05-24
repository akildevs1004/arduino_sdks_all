// #define ARDUHAL_LOG_LEVEL 0  // Disable WiFi/HTTP debug logs
//Model ESP32-WROOM-DA Module
// #include <ESPmDNS.h>

#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
// #include <WiFiManager.h>
#include "PCF8574.h"
#include <HTTPClient.h>
// #include <ArduinoJson.h>
#include <WiFiClient.h>
// #include <Update.h>
#include <ArduinoOTA.h>
#include <ModbusMaster.h>
// #include <WiFiClientSecure.h>



#define DEBUG 0  // Set to 1 to enable debug
// WiFiManager wifiManager;
DynamicJsonDocument config(1024);  // Allocate 1024 bytes for JSON storage
String sessionToken = "";

bool loginStatus = false;
//  WiFiClientSecure client;
WiFiClient client;  // Create a client object
WebServer server(80);
String deviceConfigContent = "";
String sensorData = "";
String DeviceIPNumber = "";
String loginErrorMessage = "";
String GlobalWebsiteResponseMessage = "";
String GlobalWebsiteErrorMessage = "";
HTTPClient http;
int cloudAccountActiveDaysRemaining = 100;
unsigned long lastRun = 0;
const unsigned long interval = 24UL * 60UL * 60UL * 1000UL;  // 24 hours in milliseconds
String serverURL = "";
String todayDate = "";
String device_serial_number = "XT123456";
bool USE_ETHERNET = true;
bool USE_DEFAULT_WIFIMANGER = false;
String firmWareVersion = "2.1";

bool loadingConfigFile = false;

void setup() {
  Serial.begin(115200);



  Serial.printf("Flash size (bytes): %u\n", ESP.getFlashChipSize());


  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS is Not available");

    delay(1000);
  } else {
    ensureConfigExists();
    delay(200);
    String savedData = readConfig("config.json");
    Serial.println(savedData);
    if (savedData != "") {
      deserializeJson(config, savedData);
      if (config["wifi_or_ethernet"].as<String>() == "1")
        USE_ETHERNET = false;


      serverURL = config["server_url"].as<String>();
    }


    DeviceSetup();
  }
  if (USE_DEFAULT_WIFIMANGER) {

    WiFi.mode(WIFI_STA);  // Ensure station mode

    connectDefaultWifiAuto();

  } else {
    startNetworkProcessStep1();  //load config and start Device web server
  }

  lastRun = millis();  // Initial timestamp

  routes();  // Define Routes and handlers
  server.begin();
  Serial.println("HTTP Server started");
  Serial.print("------------------------------------------------");
  Serial.println(WiFi.status());


  if (WiFi.status() == WL_CONNECTED || USE_ETHERNET) {

    delay(1000);
    updateJsonConfig("config.json", "ipaddress", DeviceIPNumber);
    updateJsonConfig("config.json", "firmWareVersion", firmWareVersion);
    updateJsonConfig("config.json", "internet", "online");

    // configTime(0, 0, "pool.ntp.org");
    // delay(2000);  // Wait for NTP sync

    // // // Get today's date
    // todayDate = getCurrentDate();
    // Serial.println("Today's Date: " + todayDate);

    Serial.println("-----------------------socketConnectServer--------------------------------------------");
    socketConnectServer();
    Serial.println("-----------------------socketConnectServer--------------------------------------------");

    handleHeartbeat();
    Serial.println("-----------------------handleHeartbeat--------------------------------------------");

    getDeviceAccountDetails();
    Serial.println("-----------------------getDeviceAccountDetails--------------------------------------------");

    updateFirmWaresetup();
    Serial.println("-----------------------updateFirmWaresetup--------------------------------------------");

    uploadHTMLsetup();
    Serial.println("-----------------------uploadHTMLsetup--------------------------------------------");

    cloudAccountActiveDaysRemaining = 100;
    if (cloudAccountActiveDaysRemaining <= 0) {
      Serial.println("❌ XXXXXXXXXXXXXXXXXXXXXXXXXXXXX----Account is expired----XXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    }
  }
}

void loop() {
  // if (!loadingConfigFile) //do not load while saving configuration file
  {

    server.handleClient();
    if (WiFi.status() == WL_CONNECTED || USE_ETHERNET) {

      if (config["internet"] != "online")
        updateJsonConfig("config.json", "internet", "online");

      if (cloudAccountActiveDaysRemaining > 0) {

        handleHeartbeat();
        updateFirmWareLoop();

        unsigned long currentMillis = millis();


        if (currentMillis - lastRun >= interval) {
          lastRun = currentMillis;
          // getDeviceAccoutnDetails();
        }
      }

      // deviceReadSensorsLoop();
      delay(200);  // Non-blocking delay
    } else {
      if (config["internet"] != "offline")
        updateJsonConfig("config.json", "internet", "offline");
    }

    Deviceloop();
  }
}

String replaceHeaderContent(String html) {


  // Read saved data
  String savedData = readConfig("config.json");


  String field1Value = "";

  if (savedData != "") {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, savedData);
    html.replace("{config_json}", savedData);
  }








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



void ensureConfigExists() {
  const char* CONFIG_PATH = "/config.json";
  const char* DEFAULT_CONFIG_PATH = "/default_config.json";
  // If config.json does not exist, copy from default_config.json
  if (!LittleFS.exists(CONFIG_PATH)) {
    Serial.println("config.json not found, creating from default_config.json...");

    File defaultFile = LittleFS.open(DEFAULT_CONFIG_PATH, "r");
    if (!defaultFile) {
      Serial.println("Failed to open default_config.json");
      return;
    }

    String defaultData = defaultFile.readString();
    defaultFile.close();

    File configFile = LittleFS.open(CONFIG_PATH, "w");
    if (!configFile) {
      Serial.println("Failed to create config.json");
      return;
    }

    configFile.print(defaultData);
    configFile.close();



    Serial.println("config.json created from default_config.json.");

    delay(2000);  // Ensure write finishes
    ESP.restart();
  } else {
    Serial.println("config.json already exists.");
  }
}
void restoreDefaultConfig() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  File defaultFile = LittleFS.open("/default_config.json", "r");
  if (!defaultFile) {
    Serial.println("Default config file not found");
    return;
  }

  String defaultData = defaultFile.readString();
  defaultFile.close();

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config.json for writing");
    return;
  }

  configFile.print(defaultData);
  configFile.close();

  Serial.println("Configuration restored to default.");

  delay(2000);  // Ensure write finishes
  ESP.restart();
}
