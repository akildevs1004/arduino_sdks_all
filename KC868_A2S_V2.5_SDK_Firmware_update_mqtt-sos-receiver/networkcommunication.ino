
#define ETH_PHY_ADDR 0
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_POWER_PIN -1
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_TYPE ETH_PHY_LAN8720


//---------------------------NETWORK SETTINGS START------------------------------------------

// Static IP Configuration
IPAddress local_IP = IPAddress();
IPAddress gateway = IPAddress();
IPAddress subnet = IPAddress();
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);


// Simple credentials (replace with secure method for production)
const char* USERNAME = "admin";
const char* PASSWORD = "password";


String WIFI_SSID = "";
String WIFI_PASSWORD = "";


IPAddress wifi_local_IP(192, 168, 1, 200);  // Using .100 as requested
IPAddress wifi_gateway(192, 168, 1, 1);
IPAddress wifi_subnet(255, 255, 255, 0);
IPAddress wifi_primaryDNS(8, 8, 8, 8);
IPAddress wifi_secondaryDNS(8, 8, 4, 4);

bool wifiConnected = false;




unsigned long lastInternetCheck = 0;
const unsigned long checkInterval = 60 * 60 * 1000;  // 1 hour = 60*60*1000 ms
void networkSetup1() {
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

    // if (config["socket_communication"])
    //   socketConnectServer();
    // else
    //   updateJsonConfig("config.json", "socket", "offline");
    handleHeartbeat();
    //getDeviceAccountDetails();

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
}
void networkLoop() {

  networkStatusLoop();
  unsigned long nowInternet = millis();

  if (nowInternet - lastInternetCheck >= checkInterval) {
    lastInternetCheck = nowInternet;

    checkInternetConnection();

    if (!hasInternet || config["mqtt"] == "offline") {
      Serial.println("🔁 Restarting due to no internet...");
      delay(2000);
      ESP.restart();
    }
  }
}
void networkStatusLoop() {
  if (WiFi.status() == WL_CONNECTED || USE_ETHERNET) {

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
}

int heartBeatSeconds = 20;
unsigned long previousHeartbeatMillis = 0;  // Stores last time heartbeat was sent
void handleHeartbeat() {

  // Serial.print("Heartbeat ");
  // Serial.println(config["heartbeat"].as<int>());

  heartBeatSeconds = config["heartbeat"].as<int>();

  if (heartBeatSeconds <= 5) {
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





    // if (config["socket_communication"])
    //   socketDeviceHeartBeatToServer(heartbeatData);

    if (config["mqtt_communication"])

    {
      DynamicJsonDocument heartbeatDoc2(2048);
      heartbeatDoc2["serialNumber"] = config["device_serial_number"];
      heartbeatDoc2["type"] = "heartbeat";
      heartbeatDoc2["timestamp"] = millis();
      //heartbeatDoc2["config"] = readConfig("config.json");  //deviceConfigContent;  // ////////readConfig("config.json");
      //heartbeatDoc["sensor_data"] = sensorData;      // ////////readConfig("config.json");
      serializeJson(heartbeatDoc2, heartbeatData);
      mqttHeartBeat(heartbeatData);
    }
  }

  // if (config["socket_communication"]) {
  //   processSocketServerRequests();  //loop like get config  Update Config

  // } else {
  // }

  // unsigned long currentMillisSocket = millis();

  // if (currentMillisSocket - previousHeartbeatMillisSocket >= 1 * 1000) {
  //   previousHeartbeatMillisSocket = currentMillisSocket;
  //   processSocketServerRequests();
  // }
}


//---------------------------NETWORK SETTINGS END------------------------------------------
void startNetworkProcessStep1() {


  Serial.print("USE_ETHERNET----------------------");
  Serial.println(USE_ETHERNET);

  configureWifiEtherNetServer();  //server start
}
void configureWifiEtherNetServer() {
  // Apply configuration
  if (USE_ETHERNET) {



    // 1) Start ETH (DHCP)
    if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
      Serial.println("[ETH] Failed to start");
      return;
    }

    // 2) Wait for link + (optional) DHCP
    uint32_t t0 = millis();
    while (!ETH.linkUp() && millis() - t0 < 10000) delay(50);
    if (!ETH.linkUp()) {
      Serial.println("[ETH] Link timeout");
      return;
    }

    delay(1000);
    t0 = millis();
    while (ETH.localIP() == INADDR_NONE && millis() - t0 < 10000) delay(50);

    IPAddress ip0 = ETH.localIP();
    Serial.print("[ETH] Initial IP: ");
    Serial.println(ip0);

    Serial.println(ip0[0]);
    Serial.println(ip0[1]);
    Serial.println(ip0[2]);


    Serial.println(ip0[3]);
    Serial.println(config["eth_ip"].as<String>());

    if (config["eth_ip"].as<String>() == "0.0.0.0" || ip0 == "0.0.0.0") {
      Serial.println("Failed to connect. Restarting...");
      delay(3000);
      return;
    }
    // 3) If config.eth_ip is empty → force 192.168.N.200 (reuse N from DHCP)


    String eth_ip_str = config["eth_ip"].as<String>();

    IPAddress ipStored;
    ipStored.fromString(eth_ip_str);

    bool invalidConfig =
      !config.containsKey("eth_ip") || ip0[2] != ipStored[2] || eth_ip_str.isEmpty() || eth_ip_str == "undefined" || eth_ip_str == "null" || eth_ip_str == "0.0.0.0" || eth_ip_str == "192.168.0.200";

    bool invalidRuntime =
      (ip0[0] == 0 && ip0[1] == 0 && ip0[2] == 0 && ip0[3] == 0);
    int N = ip0[2];

    //if Config Ethernet is Empty
    if (invalidConfig || invalidRuntime) {
      Serial.println("[ETH] Invalid IP or configuration detected — Default .200 ");
      //delay(1000);
      //ESP.restart();
      //}



      //if (config["eth_ip"].as<String>().isEmpty() || config["eth_ip"].as<String>() == "undefined"  || config["eth_ip"].as<String>() == "null" || ip0  == "0.0.0.0" || config["eth_ip"].as<String>() == "0.0.0.0") {
      // if (ip0[0] == 192 && ip0[1] == 168)
      // {
      delay(1000);

      Serial.println(N);
      // If already .200, nothing to do
      if (ip0[3] != 200 && N != 0) {

        IPAddress newIP(192, 168, N, 200);

        // Prefer DHCP-provided gateway if it matches 192.168.N.*
        IPAddress dhcpGW = ETH.gatewayIP();
        IPAddress dhcpMask = ETH.subnetMask();
        IPAddress dhcpDNS1 = ETH.dnsIP(0);
        IPAddress dhcpDNS2 = ETH.dnsIP(1);

        IPAddress gw = (dhcpGW[0] == 192 && dhcpGW[1] == 168 && dhcpGW[2] == N)
                         ? dhcpGW
                         : IPAddress(192, 168, N, 1);

        IPAddress mask = (dhcpMask != INADDR_NONE) ? dhcpMask : IPAddress(255, 255, 255, 0);
        if (dhcpDNS1 == INADDR_NONE) dhcpDNS1 = gw;  // router as DNS if missing
        if (dhcpDNS2 == INADDR_NONE) dhcpDNS2 = IPAddress(1, 1, 1, 1);

        Serial.printf("[ETH] eth_ip empty → forcing %s\n", newIP.toString().c_str());
        if (!ETH.config(newIP, gw, mask, dhcpDNS1, dhcpDNS2)) {
          Serial.println("[ETH] Failed to apply static; keeping DHCP IP.");
        } else {
          Serial.print("[ETH] Forced IP now: ");
          Serial.println(ETH.localIP());


          updateJsonConfig("config.json", "eth_ip", newIP.toString().c_str());
          updateJsonConfig("config.json", "eth_gateway", gw.toString().c_str());
          updateJsonConfig("config.json", "eth_subnet", mask.toString().c_str());
        }
      } else {
        Serial.println("[ETH] Already on .200; no change needed.");
      }
      // } else {
      //   // Not a 192.168.*.* network → leave DHCP as-is
      //   Serial.println("[ETH] Not 192.168.x.x; leaving DHCP IP unchanged.");
      // }
    } else {
      local_IP.fromString(config["eth_ip"].as<String>());
      gateway.fromString(config["eth_gateway"].as<String>());
      subnet.fromString(config["eth_subnet"].as<String>());

      DeviceIPNumber = config["eth_ip"].as<String>();

      ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

      delay(2000);
      // Your existing Ethernet setup code...
      if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
        Serial.println("Ethernet Failed to Start");
      }




      if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
        Serial.println("Ethernet Failed to Start");
        return;
      }


      delay(5000);
      // Apply static IP configuration
      if (!ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("Failed to configure Ethernet with static IP");
      } else {
        Serial.println("Ethernet Static IP: ");
        Serial.println(ETH.localIP());
        DeviceIPNumber = ETH.localIP().toString();

        isNetworkConnected = true;
        DeviceIPNumber = ETH.localIP().toString();
        isNetworkConnected = true;
        Serial.print("[ETH] Final IP: ");
        Serial.println(DeviceIPNumber);

        WiFi.onEvent(onEthEvent);
      }
    }


    WiFi.onEvent(onEthEvent);

    // return true;
  } else {

    connectWifiInternet();
    WiFi.onEvent(onWiFiEvent);
    isNetworkConnected = true;
  }
}
/*
void configureWifiEtherNetServer() {
  // Apply configuration
  if (USE_ETHERNET) {

    local_IP.fromString(config["eth_ip"].as<String>());
    gateway.fromString(config["eth_gateway"].as<String>());
    subnet.fromString(config["eth_subnet"].as<String>());

    DeviceIPNumber = config["eth_ip"].as<String>();

    ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);

    delay(2000);
    // Your existing Ethernet setup code...
    if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
      Serial.println("Ethernet Failed to Start");
    }


    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
      Serial.println("An error occurred while mounting LittleFS");
      return;
    }

  


    if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
      Serial.println("Ethernet Failed to Start");
      return;
    }

     
    delay(5000);
    // Apply static IP configuration
    if (!ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Failed to configure Ethernet with static IP");
    } else {
      Serial.println("Ethernet Static IP: ");
      Serial.println(ETH.localIP());
      DeviceIPNumber = ETH.localIP().toString();
    }

    isNetworkConnected = true;

    WiFi.onEvent(onEthEvent);
    
  } else {

    connectWifiInternet();
    WiFi.onEvent(onWiFiEvent);
    isNetworkConnected = true;
  }
} */
void connectWifiInternet() {
  String ssid = config["wifi_ssid"] | "";
  String pass = config["wifi_password"] | "";
  String ipStr = config["wifi_ip"] | "";
  String primaryDnsStr = config["primary_dns"] | "8.8.8.8";
  String secondaryDnsStr = config["secondary_dns"] | "8.8.4.4";

  if (ssid.isEmpty()) {
    Serial.println("[WiFi] Missing SSID in config!");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(true, true);
  delay(200);

  Serial.printf("[WiFi] Connecting to '%s' (DHCP)...\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("[WiFi] DHCP connect failed");
    return;
  }

  IPAddress dhcpIP = WiFi.localIP();
  IPAddress dhcpGateway = WiFi.gatewayIP();
  IPAddress dhcpSubnet = WiFi.subnetMask();

  Serial.print("[WiFi] DHCP IP: ");
  Serial.println(dhcpIP);
  Serial.print("[WiFi] Gateway: ");
  Serial.println(dhcpGateway);
  Serial.print("[WiFi] Subnet : ");
  Serial.println(dhcpSubnet);

  // ---- If wifi_ip is empty, generate static IP = 192.168.X.200 ----
  IPAddress staticIP;
  bool customStatic = staticIP.fromString(ipStr);

  if (!customStatic || ipStr == "0.0.0.0" || ipStr == "undefined" || ipStr == "null") {
    // derive from gateway or DHCP IP
    uint8_t X = dhcpIP[2];
    staticIP = IPAddress(192, 168, X, 200);
    Serial.print("[WiFi] Auto-generated static IP: ");
    Serial.println(staticIP);
  }

  IPAddress dns1, dns2;
  if (!dns1.fromString(primaryDnsStr)) dns1 = IPAddress(8, 8, 8, 8);
  if (!dns2.fromString(secondaryDnsStr)) dns2 = IPAddress(8, 8, 4, 4);

  // ---- Reconnect with Static IP ----
  Serial.println("[WiFi] Reconnecting with static IP...");
  WiFi.disconnect(true);
  delay(500);

  if (!WiFi.config(staticIP, dhcpGateway, dhcpSubnet, dns1, dns2)) {
    Serial.println("[WiFi] Failed to apply static IP config!");
    return;
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("[WiFi] Static IP reconnect failed");
    return;
  }

  DeviceIPNumber = WiFi.localIP().toString();
  updateJsonConfig("config.json", "wifi_ip", DeviceIPNumber.c_str());
  Serial.println("[WiFi] Connected successfully with Static IP");
  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway    : ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Subnet     : ");
  Serial.println(WiFi.subnetMask());
}




// Called when Ethernet is up
void onEthEvent(WiFiEvent_t event) {
  switch (event) {

    case ARDUINO_EVENT_ETH_START:
      Serial.println("🔌 Ethernet Started");
      delay(5000);

      isNetworkConnected = true;
      mqttsetup();
      checkInternetConnection();
      // ETH.setHostname("esp32-ethernet");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("✅ Ethernet Connected");
      isNetworkConnected = true;
      checkInternetConnection();
        publishConfigToMQTT();
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("🌐 Ethernet IP: ");
      Serial.println(ETH.localIP());
      DeviceIPNumber = ETH.localIP().toString();
      delay(5000);
      mqttsetup();
      isNetworkConnected = true;
      checkInternetConnection();
        publishConfigToMQTT();
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("⛔ Ethernet Stopped");
      isNetworkConnected = false;
      hasInternet = false;
      pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");



      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("❌ Ethernet Disconnected");
      isNetworkConnected = false;
      hasInternet = false;
      pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");



      break;

    default:
      Serial.println("❌ No Update on Network status------------------");
      isNetworkConnected = false;
      hasInternet = false;
      pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");



      break;
  }
}

// Called when Wi-Fi is up
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_READY:
      Serial.println("📶 Wi-Fi Ready");
      delay(5000);
      mqttsetup();
      isNetworkConnected = true;
      checkInternetConnection();

      break;

      // case ARDUINO_EVENT_WIFI_CONNECTED:
      //   Serial.println("✅ Wi-Fi Connected to AP");
      //   isNetworkConnected = true;

      //   break;


    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("✅ Wi-Fi Connected");
      isNetworkConnected = true;
      checkInternetConnection();


      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("📶 Wi-Fi IP: ");
      Serial.println(WiFi.localIP());
      delay(5000);

      checkInternetConnection();
      mqttsetup();
      isNetworkConnected = true;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("❌ Wi-Fi Disconnected");
      isNetworkConnected = false;
      hasInternet = false;
      pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");



      break;

    default:
      Serial.println("❌ No Update on Network status------------------");
      isNetworkConnected = false;
      hasInternet = false;
      pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");


      break;
  }
}

void checkInternetConnection() {
  // if (!isNetworkConnected) {

  //   Serial.println("🌐 ❌ No Network");
  //   hasInternet = false;
  //   return;
  // }

  HTTPClient http;
  http.setConnectTimeout(3000);
  http.begin("http://clients3.google.com/generate_204");
  int code = http.GET();
  http.end();

  if (code == 204) {
    // if (!hasInternet)
    Serial.println("🌍 ✅  Internet Available");
    hasInternet = true;
    pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, LOW);  // LOW = ON
    updateJsonConfig("config.json", "internet", "online");
    updateJsonConfig("config.json", "relay5", "true");

digitalWrite(RELAY1_NETWORK_STATUS_PIN, HIGH);


  } else {
    // if (hasInternet)
    Serial.println("🌐 ❌ Internet Lost");
    hasInternet = false;
    pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
    updateJsonConfig("config.json", "internet", "offline");
    updateJsonConfig("config.json", "relay5", "false");

    digitalWrite(RELAY1_NETWORK_STATUS_PIN, LOW);
  }
}




void connectDefaultWifiAuto() {

  // bool res;
  // res = wifiManager.autoConnect(device_serial_number.c_str());  // SSID and Password for AP

  // if (!res) {
  //   Serial.println("Failed to connect. Restarting...");
  //   delay(3000);
  //   ESP.restart();
  // }
  // DeviceIPNumber = WiFi.localIP().toString();
  // // Connected successfully
  //  Serial.println("Connected to WiFi!");
  // Serial.println(WiFi.localIP());

  // //handleRestartDevice();
}

String getWiFiStatus() {
  switch (WiFi.status()) {
    case WL_CONNECTED: return "Connected (IP: " + WiFi.localIP().toString() + ")";
    case WL_NO_SSID_AVAIL: return "Network not available";
    case WL_CONNECT_FAILED: return "Connection failed";
    case WL_IDLE_STATUS: return "Idle";
    case WL_DISCONNECTED: return "Disconnected";
    default: return "Unknown status";
  }
}

String getEthernetStatus() {
  return ETH.linkUp() ? "Connected (IP: " + ETH.localIP().toString() + ")" : "Disconnected";
}

bool checkInternet() {
  //WiFiClient client;
  return client.connect("www.google.com", 80);
}

String getInternetStatus() {
  return checkInternet() ? "Online" : "Offline";
}