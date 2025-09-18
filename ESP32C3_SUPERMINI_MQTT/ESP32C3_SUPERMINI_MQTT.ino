#include <WiFi.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <ModbusMaster.h>
// Global objects
HTTPClient http;
WiFiManager wifiManager;
WebServer server(80);
DynamicJsonDocument jsonConfig(1024);

// MQTT Broker
String mqtt_server = "broker.hivemq.com";
int mqtt_port = 1883;
String clientId = "xtremevision_switch";
String mqtt_clientId = "xtremevision_switch";

// Pin and configuration variables
const int switchPin = 3;
const int LED_PIN = 8;
String serialNumber = "ESP32T001";  // Default Serial Number

String device_serial_number = "";

String serverURL, wifiSSID, wifiPassword, ipAddress;

const char* USERNAME = "admin";
  String PASSWORD = "password";

String temperatureThreshold = "30";
int previousSwitchState = HIGH;
bool shouldSaveConfig = false;
String socketConnectionStatus = "Disconencted";

// Replace with your server's IP address and port
String server_ip = "";    // Server IP address
String server_port = "";  // Server port number

String gmtTimeZone = "";

// IP configuration
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
String deviceIPaddress = "";

int intervalHeartbeat = 20;  // Interval at which to send heartbeat (10 seconds)
bool InternetStatus = false;
// Function declarations
bool saveConfig();
bool loadConfig();
void saveConfigCallback();
void handleRoot();
void handleSaveConfig();
void sendSwitchStatus(int status);
void wifiManagerSetup();
void listLittleFSFiles();
void handleConfigJson();
void connectToWiFi();
void setStaticIP();
void safeRestart();
bool socketConnectServer();
void socketDeviceHeartBeatToServer();
bool testInternet();


String firmWareVersion = "1.1";
const char* ssid = "akil";
// const char* password = "Akil1234";


WiFiClient client;  // Create a client object

void setup() {



  Serial.begin(115200);  // Start the Serial communication at 115200 baud rate
  delay(1000);

  pinMode(switchPin, INPUT_PULLUP);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Error mounting LittleFS. Restarting...");
    safeRestart();
  }

  if (!loadConfig()) {
    Serial.println("Error loading config. Resetting WiFi credentials.");
    wifiManager.resetSettings();
  }

  connectToWiFi();
  Serial.println("Web server started at: --------------------" + WiFi.localIP().toString());
  String ipStr = WiFi.localIP().toString();

  updateJsonConfig("config.json", "ipAddress", ipStr.c_str());
  sendSwitchStatus(digitalRead(switchPin));

  logmessage("Web server started at: " + WiFi.localIP().toString());

  // Initialize Web Server
  routesSetup();
  server.begin();
  logmessage("Web server started at: " + WiFi.localIP().toString());

  // logmessage("Hello Initialize");


  pinMode(LED_PIN, OUTPUT);

  // socketConnectServer();

  InternetStatus = testInternet();
  if (InternetStatus) {
    digitalWrite(LED_PIN, HIGH);
    mqttsetup();
  }

  else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("XXXXXXXXXXXXXXXXXXXXXXXX No Internet Connection XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  }
  updateFirmWaresetup();

  Serial.print("Version--------------------------------------------------------------------------");
  Serial.println(firmWareVersion);


  
}

void loop() {
  server.handleClient();
  handleSwitchState();

  mqttloop();
  handleHeartbeat();
  delay(1000);  // Non-blocking delay


  networkLoop();
}


void handleSwitchState() {
  int switchState = !digitalRead(switchPin);

  if (switchState != previousSwitchState) {
    sendSwitchStatus(switchState);
    previousSwitchState = switchState;
  }
}
void blinkBlueLight(int times = 1, int on_ms = 150, int off_ms = 150) {
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(on_ms);
    digitalWrite(LED_PIN, LOW);
    delay(off_ms);
  }
}




void connectToWiFi() {
  Serial.println("Connecting to WiFi...");

  if (!wifiSSID.isEmpty() && !wifiPassword.isEmpty()) {

    WiFi.mode(WIFI_OFF);
    delay(1000);
    //This line hides the viewing of ESP as wifi hotspot
    WiFi.mode(WIFI_STA);


    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    int retryCount = 0;

    while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
      retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi!");

      digitalWrite(LED_PIN, HIGH);



      updateJsonConfig("config.json", "wifiConnection", "Connected");

      // setStaticIP();

    } else {

      digitalWrite(LED_PIN, LOW);

      Serial.println("Failed to connect. Starting WiFiManager...");

      updateJsonConfig("config.json", "wifiConnection", "Disconnected");

      Serial.println("\n❌ Failed to connect, starting AP mode...");

      WiFi.mode(WIFI_AP);
      if (WiFi.softAP(wifiSSID.c_str())) {
        Serial.println("✅ AP started!");
        Serial.print("AP SSID: ");
        Serial.println(wifiSSID.c_str());
        Serial.print("AP IP Address: ");
        Serial.println(WiFi.softAPIP());
      } else {
        Serial.println("❌ Failed to start AP!");
      }





      wifiManagerSetup();
    }
  } else {
    Serial.println("EMPTY Password");

    wifiManagerSetup();


    //  if (wifiPassword.isEmpty() || jsonConfig["wifiPassword"].as<String>()=="")
    // {
    //   IPAddress currentIP = WiFi.localIP();
    //   // Derive base from current network, force .100
    //   IPAddress newIP(currentIP[0], currentIP[1], currentIP[2], 100);
    //   IPAddress subnet(255, 255, 255, 0);
    //   IPAddress gw(currentIP[0], currentIP[1], currentIP[2], 1);  // typical router
    //   IPAddress dns(8, 8, 8, 8);

    //   if (WiFi.config(newIP, gw, subnet, dns)) {
    //     Serial.print("🔧 Forced Static IP: ");
    //     Serial.println(WiFi.localIP());
    //     updateJsonConfig("config.json", "ipAddress", WiFi.localIP().toString().c_str());
    //   } else {
    //     Serial.println("❌ Failed to set forced static IP (.100)");
    //   }


    // }
  }
}

void setStaticIP() {

  // String ipStr =   jsonConfig["ipAddress"].as<String>();;
  String ipStr = jsonConfig["ipAddress"].as<String>();
  ;
  if (ipStr.isEmpty()) {
    Serial.println("⚠️ No static IP in config → keep DHCP.");
    return;
  }

  IPAddress cfgIP;
  if (!cfgIP.fromString(ipStr)) {
    Serial.println("❌ Invalid static IP in config → keep DHCP.");
    return;
  }

  IPAddress currentIP = WiFi.localIP();

  Serial.println(currentIP);
  Serial.println(cfgIP);


  if (currentIP == cfgIP) {
    Serial.println("ℹ️ Current IP already matches config IP → nothing to change.");
    return;
  }

  // Defaults: no gateway, no DNS, /24 subnet
  IPAddress gw(0, 0, 0, 0), subnet(255, 255, 255, 0), dns(0, 0, 0, 0);

  if (WiFi.config(cfgIP, gw, subnet, dns)) {
    Serial.println("✅ Changed to config IP:");
    Serial.print("  Old IP: ");
    Serial.println(currentIP);
    Serial.print("  New IP: ");
    Serial.println(WiFi.localIP());

    // updateJsonConfig("config.json", "ipAddress", WiFi.localIP().toString().c_str());

    String ipStr = WiFi.localIP().toString();
    updateJsonConfig("config.json", "ipAddress", ipStr.c_str());  // pointer stays va
  } else {
    Serial.println("❌ Failed to apply config IP → keeping DHCP IP.");
  }



  // IPAddress currentIP = WiFi.localIP();
  // IPAddress newIP(currentIP[0], currentIP[1], currentIP[2], 100);
  // IPAddress newGateway(currentIP[0], currentIP[1], 1, 1);

  // if (!WiFi.config(newIP, newGateway, subnet)) {
  //   Serial.println("Failed to configure static IP");
  // } else {
  //   Serial.println("New Static IP: " + WiFi.localIP().toString());


  //   deviceIPaddress = WiFi.localIP().toString();
  // }
}

void wifiManagerSetup() {
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  if (!wifiManager.autoConnect(serialNumber.c_str())) {
    Serial.println("WiFiManager failed. Restarting...");
    delay(3000);
    safeRestart();
  }

  wifiSSID = WiFi.SSID();
  wifiPassword = "";  // WiFi password is typically not retrievable

  if (shouldSaveConfig) {
    saveConfig();
  }
}

void sendSwitchStatus(int status) {



  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send switch status.");
    return;
  }
  DynamicJsonDocument doc(512);
  //doc["serialNumber"] = serialNumber;
  // doc["type"] = "switchStatus";
  //doc["switchStatus"] = status;  // keep as number/bool (no String())
  doc["room_number"] = serialNumber;
  doc["status"] = status;                                   // avoid duplicating semantics unless
  doc["ipAddress"] = jsonConfig["ipAddress"].as<String>();  // avoid duplicating semantics unless



  String body;
  serializeJson(doc, body);

  if (WiFi.status() == WL_CONNECTED) {



    ///MQTT

    mqttPublishMessage(body);



    HTTPClient http;
    http.begin(serverURL);   // for HTTPS: use WiFiClientSecure and set cert/INSECURE
    http.setTimeout(10000);  // 10s timeout
    http.addHeader("Content-Type", "application/json");
    Serial.println("serverURL: " + serverURL);

    Serial.println("POST JSON: " + body);
    int code = http.POST(body);

    String resp = http.getString();
    Serial.printf("HTTP %d\n", code);
    Serial.println("Response: " + resp);

    if (code > 0 && code < 400) {
      blinkBlueLight(2);  // success blink
    } else {
      blinkBlueLight(5, 70, 70);  // error blink
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }


  /*
  DynamicJsonDocument swtchDoc(1024);
  swtchDoc["serialNumber"] = serialNumber;
  swtchDoc["type"] = "switchStatus";
  //swtchDoc["config"] = jsonConfig;
  swtchDoc["switchStatus"] = String(status);

  swtchDoc["room_number"] = serialNumber;
  swtchDoc["status"] = String(status);


  String swichData;
  serializeJson(swtchDoc, swichData);

  client.println(swichData);
  Serial.println("Sent Swtch Data: " + swichData);
  Serial.println("serverURL: " + String(serverURL));
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  //String postData = "serial_number=" + serialNumber + "&room_number=" + serialNumber + "&status=" + String(status);
  int httpResponseCode = http.POST(swichData);

  if (httpResponseCode > 0) {
    Serial.println("HTTP Response code: " + String(httpResponseCode));


    blinkBlueLight();

     String payload = http.getString();
  Serial.println("Response: " + payload);

  } else {
    String payload = http.getString();
  Serial.println("Response: " + payload);

    Serial.println("Error in HTTP POST: " + String(httpResponseCode));
  }

  http.end();*/
}


// void Serial.println(String message)
// {
//   logmessage(message);
// }

void logmessage(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send switch status.");
    return;
  }


  /*
  client.println(message);
  Serial.println("message " + message);
  Serial.println("serverURL: " + String(serverURL));
  http.begin("https://alarmbackend.xtremeguard.org/api/testappendtext");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postData =  "message=" + message;
  int httpResponseCode = http.POST(postData);
    //httpResponseCode = http.GET();


  if (httpResponseCode > 0) {
    Serial.println("HTTP Response code: " + String(httpResponseCode));
  } else {
    Serial.println("Error in HTTP POST: " + String(httpResponseCode));
  }

  http.end();
  */
}


bool testInternet() {
  uint16_t timeoutMs = 5000;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected");
    return false;
  }

  // // 1) DNS resolve test
  // IPAddress resolved;
  // const char* host = "example.com";  // simple, fast to resolve
  // bool dnsOK = (WiFi.hostByName(host, resolved) == 1 && resolved != (uint32_t)0);
  // Serial.printf("%s DNS resolve: %s -> %s\n",
  //               dnsOK ? "✅" : "❌",
  //               host,
  //               dnsOK ? resolved.toString().c_str() : "(failed)");
  // if (!dnsOK) return false;  // no internet without DNS (for most cases)

  // // 2) HTTP 204 connectivity check (very small, fast)
  // // Google’s connectivity endpoint usually returns 204 if the internet is reachable
  // // Use HTTP (not HTTPS) to avoid TLS complexity on tiny checks
  HTTPClient http;
  http.setTimeout(timeoutMs);
  const char* probe = "http://clients3.google.com/generate_204";
  if (!http.begin(probe)) {
    Serial.println("❌ HTTP begin() failed");
    return false;
  }
  int code = http.GET();  // should be 204 if OK
  http.end();

  bool httpOK = (code == 204);
  Serial.printf("%s HTTP 204 check [%s] -> code: %d\n",
                httpOK ? "✅" : "❌", probe, code);

  // 3) (Optional) ICMP ping the resolved IP (uncomment if ESP32Ping is installed)
  /*
  bool pingOK = Ping.ping(resolved, 2); // 2 attempts
  Serial.printf("%s Ping %s\n", pingOK ? "✅" : "❌", resolved.toString().c_str());
  if (!pingOK) return false;
  */

  return httpOK;
}
