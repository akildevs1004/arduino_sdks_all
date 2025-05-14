
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


    // Your existing Ethernet setup code...
    if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
      Serial.println("Ethernet Failed to Start");
    }


    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
      Serial.println("An error occurred while mounting LittleFS");
      return;
    }

    WiFi.onEvent(WiFiEvent);

    if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
      Serial.println("Ethernet Failed to Start");
      return;
    }

    //   if ( ETH.linkStatus()==ETH_LINK_OFF) {
    //   delay(1000);
    // }

    // Apply static IP configuration
    if (!ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Failed to configure Ethernet with static IP");
    } else {
      Serial.println("Static IP: ");
      Serial.println(ETH.localIP());
      DeviceIPNumber = ETH.localIP().toString();
    }


    // if (ETH.linkUp()) {
    //   configTime(0, 0, "pool.ntp.org");
    //   delay(2000);  // Wait for NTP sync

    //   // // Get today's date
    //   todayDate = getCurrentDate();
    //   Serial.println("Today's Date: " + todayDate);
    // }
  } else {

    connectWifiInernet();
  }
}
void connectWifiInernet() {


  String wifissid = config["wifi_ssid"].as<String>();
  String wifipassword = config["wifi_password"].as<String>();

  // Step 1: Connect with DHCP to fetch gateway & subnet
  Serial.println("Connecting with DHCP to get gateway...");
  WiFi.begin(wifissid, wifipassword);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Initial WiFi connection failed");
    return;
  }

  IPAddress gateway = WiFi.gatewayIP();
  IPAddress subnet = WiFi.subnetMask();

  Serial.print("Detected Gateway: ");
  Serial.println(gateway);
  Serial.print("Detected Subnet : ");
  Serial.println(subnet);

  // Step 2: Prepare static IP settings from config
  IPAddress staticIP, primaryDNS, secondaryDNS;

  if (!staticIP.fromString(config["wifi_ip"].as<String>())) {
    Serial.println("Invalid static IP in config");
    return;
  }

  if (!primaryDNS.fromString(config["primary_dns"] | "8.8.8.8")) {
    primaryDNS = IPAddress(8, 8, 8, 8);
  }

  if (!secondaryDNS.fromString(config["secondary_dns"] | "8.8.4.4")) {
    secondaryDNS = IPAddress(8, 8, 4, 4);
  }

  // Step 3: Disconnect, apply static IP config, and reconnect
  Serial.println("Reconnecting with static IP...");
  WiFi.disconnect(true);
  delay(500);

  bool success = WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS);
  if (!success) {
    Serial.println("Failed to set static IP config!");
    return;
  }

  WiFi.begin(wifissid, wifipassword);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi reconnection with static IP failed");
    return;
  }
  DeviceIPNumber = WiFi.localIP().toString();

  // Done!
  Serial.println("WiFi Connected with Static IP:");
  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway    : ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Subnet     : ");
  Serial.println(WiFi.subnetMask());
}


// Your existing WiFi event handler
void WiFiEvent(WiFiEvent_t event) {
  Serial.println("WiFi Event Occurred");
  // Add specific event handling if needed
}


void connectDefaultWifiAuto() {

  bool res;
  res = wifiManager.autoConnect(device_serial_number.c_str());  // SSID and Password for AP

  if (!res) {
    Serial.println("Failed to connect. Restarting...");
    delay(3000);
    ESP.restart();
  }
  DeviceIPNumber = WiFi.localIP().toString();
  // Connected successfully
  Serial.println("Connected to WiFi!");
  Serial.println(WiFi.localIP());

  //handleRestartDevice();
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