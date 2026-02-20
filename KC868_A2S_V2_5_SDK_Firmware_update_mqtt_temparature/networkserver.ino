
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
const unsigned long checkInterval = 3600000;  // 1 hour = 60*60*1000 ms

void networkLoop() {


  unsigned long nowInternet = millis();

  if (nowInternet - lastInternetCheck >= checkInterval) {
    lastInternetCheck = nowInternet;

    checkInternetConnection();

    // if (!hasInternet) {
    //   Serial.println("🔁 Restarting due to no internet...");
    //   delay(2000);
    //   ESP.restart();
    // }
  }
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

    // WiFi.onEvent(WiFiEvent);





    if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
      Serial.println("Ethernet Failed to Start");
      return;
    }

    //   if ( ETH.linkStatus()==ETH_LINK_OFF) {
    //   delay(1000);
    // }
    delay(5000);
    // Apply static IP configuration
    if (!ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Failed to configure Ethernet with static IP");
    } else {
      Serial.println("Static IP: ");
      Serial.println(ETH.localIP());
      DeviceIPNumber = ETH.localIP().toString();
if(DeviceIPNumber!="0.0.0.0")
      updateJsonConfig("config.json", "eth_ip", DeviceIPNumber);
    }

    isNetworkConnected = true;

    WiFi.onEvent(onEthEvent);
    // if (ETH.linkUp()) {
    //   configTime(0, 0, "pool.ntp.org");
    //   delay(2000);  // Wait for NTP sync

    //   // // Get today's date
    //   todayDate = getCurrentDate();
    //   Serial.println("Today's Date: " + todayDate);
    // }
  } else {

    connectWifiInernet();
    WiFi.onEvent(onWiFiEvent);
    isNetworkConnected = true;
  }
}
void connectWifiInernet() {


  String wifissid = config["wifi_ssid"] | "";
  String wifipassword = config["wifi_password"] | "";

  if (wifissid.length() == 0) {
    Serial.println("WiFi SSID missing in config");
    return;
  }

  // Step 1: Connect with DHCP to fetch gateway & subnet
  Serial.println("Connecting with DHCP to get gateway...");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(200);

  Serial.print("WiFi connecting to: ");
  Serial.println(wifissid);

  WiFi.begin(wifissid.c_str(), wifipassword.c_str());

  // ✅ timeout connect (20s)
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Initial WiFi connection failed");
    return;
  }

  IPAddress gateway = WiFi.gatewayIP();
  IPAddress subnet  = WiFi.subnetMask();

  Serial.print("Detected Gateway: ");
  Serial.println(gateway);
  Serial.print("Detected Subnet : ");
  Serial.println(subnet);

  // Step 2: Prepare static IP settings from config
  IPAddress staticIP, primaryDNS, secondaryDNS;

  if (!staticIP.fromString((const char*)(config["wifi_ip"] | ""))) {
    Serial.println("Invalid static IP in config");
    return;
  }

  if (!primaryDNS.fromString((const char*)(config["primary_dns"] | "8.8.8.8"))) {
    primaryDNS = IPAddress(8, 8, 8, 8);
  }

  if (!secondaryDNS.fromString((const char*)(config["secondary_dns"] | "8.8.4.4"))) {
    secondaryDNS = IPAddress(8, 8, 4, 4);
  }

  // Step 3: Disconnect, apply static IP config, and reconnect
  Serial.println("Reconnecting with static IP...");
  WiFi.disconnect(true, true);
  delay(500);

  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to set static IP config!");
    return;
  }

  WiFi.begin(wifissid.c_str(), wifipassword.c_str());

  t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi reconnection with static IP failed");
    return;
  }

  DeviceIPNumber = WiFi.localIP().toString();
  if (DeviceIPNumber != "0.0.0.0") {
    updateJsonConfig("config.json", "wifi_ip", DeviceIPNumber);
  }

  Serial.println("WiFi Connected with Static IP:");
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
       mqttsetup();
      checkInternetConnection();
     
      break;

    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("🌐 Ethernet IP: ");
      
      Serial.println(ETH.localIP());
      if(ETH.localIP().toString()!="0.0.0.0")
      updateJsonConfig("config.json", "eth_ip", ETH.localIP().toString());
      delay(5000);
      mqttsetup();
      isNetworkConnected = true;
      checkInternetConnection();
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("⛔ Ethernet Stopped");
      isNetworkConnected = false;
      hasInternet = false;
      //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");



      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("❌ Ethernet Disconnected");
      isNetworkConnected = false;
      hasInternet = false;
      //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");



      break;

    default:
      Serial.println("❌ No Update on Network status------------------");
      isNetworkConnected = false;
      hasInternet = false;
      //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
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
      //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");



      break;

    default:
      Serial.println("❌ No Update on Network status------------------");
      isNetworkConnected = false;
      hasInternet = false;
      //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
      updateJsonConfig("config.json", "internet", "offline");
      updateJsonConfig("config.json", "relay5", "false");


      break;
  }
}

void checkInternetConnection() {

   hasInternet = false;
    //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, LOW);  // LOW = ON
    updateJsonConfig("config.json", "internet", "online");
    updateJsonConfig("config.json", "relay5", "true");

    return ;
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
    //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, LOW);  // LOW = ON
    updateJsonConfig("config.json", "internet", "online");
    updateJsonConfig("config.json", "relay5", "true");




  } else {
    // if (hasInternet)
    Serial.println("🌐 ❌ Internet Lost");
    hasInternet = false;
    //pcf8574_RE1.digitalWrite(RELAY_INTERNET_LED, HIGH);  // LOW = ON
    updateJsonConfig("config.json", "internet", "offline");
    updateJsonConfig("config.json", "relay5", "false");
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