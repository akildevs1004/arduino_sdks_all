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
DynamicJsonDocument config(4096);  // Allocate 1024 bytes for JSON storage
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
String default_device_serial_number = "XTP100002";
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


  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS is Not available");

    delay(1000);
  } else {
    Serial.println("Checking Config File is exist or not.........");
     delay(5000);
    ensureConfigExists();
    delay(200);
    String savedData = readConfig("config.json");
    Serial.println(savedData);
    if (savedData != "") {
      deserializeJson(config, savedData);
      if (config["wifi_or_ethernet"].as<String>() == "1")
        USE_ETHERNET = false;


      serverURL = config["server_url"].as<String>();



      // //config["device_serial_number"] = device_serial_number;
      // if (!config["device_serial_number"])
      //   updateJsonConfig("config.json", "device_serial_number", device_serial_number);
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


    DeviceSetup();
    digitalINPUTsSetup();

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

      if (config["socket_communication"])
        socketConnectServer();
      else
        updateJsonConfig("config.json", "socket", "offline");
      handleHeartbeat();
      getDeviceAccountDetails();

      if (config["mqtt_communication"])
        mqttsetup();
      else
        updateJsonConfig("config.json", "mqtt", "offline");

      cloudAccountActiveDaysRemaining = 100;
      if (cloudAccountActiveDaysRemaining <= 0) {
        Serial.println("❌ XXXXXXXXXXXXXXXXXXXXXXXXXXXXX----Account is expired----XXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
      }
    } else {
      updateJsonConfig("config.json", "socket", "offline");
      updateJsonConfig("config.json", "mqtt", "offline");
      updateJsonConfig("config.json", "internet", "offline");
    }

    updateFirmWaresetup();
    uploadHTMLsetup();
    checkInternetConnection();
  }
}

void loop() {
  // if (!loadingConfigFile) //do not load while saving configuration file
  {

    server.handleClient();
    if (WiFi.status() == WL_CONNECTED || USE_ETHERNET || hasInternet) {

      if (config["internet"] != "online")
        updateJsonConfig("config.json", "internet", "online");

      if (cloudAccountActiveDaysRemaining > 0) {

        handleHeartbeat();


        unsigned long currentMillis = millis();


        if (currentMillis - lastRun >= interval) {
          lastRun = currentMillis;
          // getDeviceAccoutnDetails();
        }
      }

      if (config["mqtt_communication"])
        mqttloop();

    } else {

      updateJsonConfig("config.json", "socket", "offline");
      updateJsonConfig("config.json", "mqtt", "offline");
      updateJsonConfig("config.json", "internet", "offline");
    }

    updateFirmWareLoop();
    Deviceloop();
    DitialInputloop();
    networkLoop();
  }

  delay(200);  // Non-blocking delay
}

int heartBeatSeconds = 20;
unsigned long previousHeartbeatMillis = 0;  // Stores last time heartbeat was sent
void handleHeartbeat() {

  // Serial.print("Heartbeat ");
  // Serial.println(config["heartbeat"].as<int>());

  heartBeatSeconds = config["heartbeat"].as<int>();
  if (config["heartbeat"].as<int>() < 5) {
    heartBeatSeconds = 20;
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousHeartbeatMillis >= heartBeatSeconds * 1000) {
    previousHeartbeatMillis = currentMillis;


    DynamicJsonDocument heartbeatDoc(1024);
    heartbeatDoc["serialNumber"] = config["device_serial_number"];
    heartbeatDoc["type"] = "heartbeat";
    heartbeatDoc["config"] = readConfig("config.json");
    heartbeatDoc["timestamp"] = millis();

    //deviceConfigContent;  // ////////readConfig("config.json");
    //heartbeatDoc["sensor_data"] = sensorData;      // ////////readConfig("config.json");
    String heartbeatData;
    serializeJson(heartbeatDoc, heartbeatData);





    if (config["socket_communication"])
      socketDeviceHeartBeatToServer(heartbeatData);

    if (config["mqtt_communication"] && config["device_serial_number"])

    {
      DynamicJsonDocument heartbeatDoc2(2048);
      heartbeatDoc2["serialNumber"] = config["device_serial_number"];
      heartbeatDoc2["type"] = "heartbeat";
      // heartbeatDoc2["testing"] = "yes";

      heartbeatDoc2["timestamp"] = millis();
      //heartbeatDoc2["config"] = readConfig("config.json");  //deviceConfigContent;  // ////////readConfig("config.json");
      //heartbeatDoc["sensor_data"] = sensorData;      // ////////readConfig("config.json");
      serializeJson(heartbeatDoc2, heartbeatData);
      mqttHeartBeat(heartbeatData);
    }
  }

  if (config["socket_communication"]) {
    processSocketServerRequests();  //loop like get config  Update Config

  } else {
  }

  // unsigned long currentMillisSocket = millis();

  // if (currentMillisSocket - previousHeartbeatMillisSocket >= 1 * 1000) {
  //   previousHeartbeatMillisSocket = currentMillisSocket;
  //   processSocketServerRequests();
  // }
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
