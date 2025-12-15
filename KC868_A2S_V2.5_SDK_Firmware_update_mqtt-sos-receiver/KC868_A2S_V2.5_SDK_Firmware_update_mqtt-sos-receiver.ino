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
//partition
String serverURL = "";
String todayDate = "";
String default_device_serial_number = "XTSOS251000";  //PARKING Device
String device_serial_number = "";
bool USE_ETHERNET = true;
bool USE_DEFAULT_WIFIMANGER = false;
String firmWareVersion = "1.0";

bool loadingConfigFile = false;

bool isNetworkConnected = false;
bool hasInternet = false;




void setup() {
  Serial.begin(115200);
  Serial.printf("Flash size (bytes): %u\n", ESP.getFlashChipSize());
  if (!LittleFS.begin(false)) { //false means do not format the littlefs 
    Serial.println("LittleFS is Not available");
    delay(1000);
  } else {
    Serial.println("Checking Config File is exist or not.........");
    delay(5000);
    ensureConfigExists();
    //DeviceSetup();

    delay(200);
    networkSetup1();
    updateFirmWaresetup();
    uploadHTMLsetup();
    // checkInternetConnection();
    //getDeviceAccountDetails();

// 1) init RF as you already do


// 2) add routes
setupSosApiRoutes_TwoButtons();
    //Device Setup 
    
  }
}

void loop() {
  // if (!loadingConfigFile) //do not load while saving configuration file
  {
    Serial.println("Loop Started...............");
    server.handleClient();
    Serial.println("Deviceloop Loop Started...............");
    // Deviceloop();
    Serial.println("networkLoop Loop Started...............");
    networkLoop();


    Serial.println("Firmware Loop Started...............");

    updateFirmWareLoop();
    loopSosDevice();
  }  //loop

  delay(200);  // Non-blocking delay
}




void ensureConfigExists() {
  Serial.println("ensureConfigExists or Not..........");
  const char* CONFIG_PATH = "/config.json";
  const char* DEFAULT_CONFIG_PATH = "/default_config.json";
  // If config.json does not exist, copy from default_config.json
  if (!LittleFS.exists(CONFIG_PATH)) {
    Serial.println("config.json not found, creating from default_config.json...");

    File defaultFile = LittleFS.open(DEFAULT_CONFIG_PATH, "r");
    if (!defaultFile) {
      Serial.println("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX Failed to open default_config.json XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
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

  delay(200);
  String savedData = readConfig("config.json");
  Serial.println(savedData);
  if (savedData != "") {
    deserializeJson(config, savedData);
    if (config["wifi_or_ethernet"].as<String>() == "1")
      USE_ETHERNET = false;


    serverURL = config["server_url"].as<String>();
  }
  Serial.print("Device Serial Number:" + device_serial_number + "-");
  Serial.println(String(config["device_serial_number"]));



  if (!config["device_serial_number"]) {
    updateJsonConfig("config.json", "device_serial_number", default_device_serial_number);
    delay(2000);  // Ensure write finishes
    ESP.restart();
  }

  //load serial number from config file only
  device_serial_number = config["device_serial_number"].as<String>();
}
