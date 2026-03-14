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
String default_device_serial_number = "XTSOS251005";  //SOS Device
String device_serial_number = "";
bool USE_ETHERNET = true;
bool USE_DEFAULT_WIFIMANGER = false;
String firmWareVersion = "1.5";

bool loadingConfigFile = false;

bool isNetworkConnected = false;
bool hasInternet = false;


/*
#include <RCSwitch.h>

RCSwitch rf1;

// ===== KC868-A2 PINS (from KinCony forum) =====
static const uint8_t RF_RX_PIN1 = 2;    // GPIO-1 terminal = GPIO33
static const uint8_t RELAY1_PIN1 = 15;  // Relay1 = GPIO15

// ===== SETTINGS =====
static const unsigned long ALARM_MS1 = 10000;  // 10 seconds
static const unsigned long DUP_IGNORE_MS1 = 400;

// Auto-learn on first press, then trigger on next presses
unsigned long learnedCode1 = 0;
unsigned int learnedBits1 = 0;
unsigned int learnedProto1 = 0;

unsigned long alarmUntil1 = 0;
unsigned long lastCode1 = 0;
unsigned long lastAt1 = 0;
*/

void setup() {

  Serial.begin(115200);
  Serial.printf("Flash size (bytes): %u\n", ESP.getFlashChipSize());
  if (!LittleFS.begin(false)) {  //false means do not format the littlefs
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

    // 1) init RF as you already do


    // 2) add routes
     setupSosApiRoutes_TwoButtons();

    // Enable RF receiver on ESP32
   /* rf1.enableReceive(digitalPinToInterrupt(RF_RX_PIN1));

    Serial.println("\nKC868-A2 + MX-RM-5V Receiver Ready");
    Serial.println("Press SOS button once to LEARN code, press again to TRIGGER Relay1.");

    Serial.println("Checking Receiver........");*/
    //Device Setup
  }
}

void loop() {


  // if (!loadingConfigFile) //do not load while saving configuration file
  {
    // Serial.println("Loop Started...............");
    // server.handleClient();

      networkLoop();

    // updateFirmWareLoop();
     loopSosDevice();

    /*if (!rf1.available()) return;
    // Serial.println("Receivere  Available.....");


    unsigned long code1 = rf1.getReceivedValue();
    unsigned int bits1 = rf1.getReceivedBitlength();
    unsigned int proto1 = rf1.getReceivedProtocol();
    unsigned int pulse1 = rf1.getReceivedDelay();
    rf1.resetAvailable();

    // Ignore noise
    if (code1 == 0) return;

    // Ignore rapid duplicates
    if (code1 == lastCode1 && (millis() - lastAt1) < DUP_IGNORE_MS1) return;
    lastCode1 = code1;
    lastAt1 = millis();

    Serial.print("RX code=");
    Serial.print(code1);
    Serial.print(" bits=");
    Serial.print(bits1);
    Serial.print(" proto=");
    Serial.print(proto1);
    Serial.print(" pulse=");
    Serial.println(pulse1);

    // Learn on first valid reception
    if (learnedCode1 == 0) {
      learnedCode1 = code1;
      learnedBits1 = bits1;
      learnedProto1 = proto1;

      Serial.print("LEARNED: code=");
      Serial.print(learnedCode1);
      Serial.print(" bits=");
      Serial.print(learnedBits1);
      Serial.print(" proto=");
      Serial.println(learnedProto1);
      return;
    }

    // Match (if your receiver varies, you can match only by code)
    bool isSOS1 = (code1 == learnedCode1 && bits1 == learnedBits1 && proto1 == learnedProto1);
    */
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
