// ESP32 KS868-A8M Wi-Fi or Ethernet Selection Example

#include <WiFi.h>
#include <ETH.h>

#define ETH_PHY_ADDR 0
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_POWER_PIN -1
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_TYPE ETH_PHY_LAN8720

// Set your Wi-Fi credentials
const char* WIFI_SSID = "akil";
const char* WIFI_PASSWORD = "Akil1234";

// Static IP settings for Ethernet
IPAddress local_IP(192, 168, 2, 200);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// Static IP settings for Wi-Fi
IPAddress wifi_IP(192, 168, 3, 22);
IPAddress wifi_gateway(192, 168, 3, 1);
IPAddress wifi_subnet(255, 255, 255, 0);

// Flag to choose network mode
bool USE_ETHERNET = true;  // Set true for Ethernet, false for Wi-Fi

void wifisetup() {
  Serial.begin(115200);
  delay(1000);

  if (USE_ETHERNET) {
    Serial.println("🔌 Starting Ethernet mode...");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);

    if (!ETH.begin(ETH_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
      Serial.println("❌ Ethernet failed to start.");
    } else {
      Serial.println("✅ Ethernet started. Waiting for link...");

      while (!ETH.linkUp()) {
        Serial.println("Waiting for Ethernet link...");
        delay(500);
      }

      delay(500);
      if (!ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("⚠️ Failed to apply static IP config.");
      }

      Serial.print("IP Address: ");
      Serial.println(ETH.localIP());
      delay(5000);

      //inteent connected
      ////mqttsetup();
    }
  } else {
    Serial.println("📶 Starting Wi-Fi mode...");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    delay(200);

    if (!WiFi.config(wifi_IP, wifi_gateway, wifi_subnet)) {
      Serial.println("⚠️ Failed to apply static IP for Wi-Fi.");
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      Serial.println("✅ Wi-Fi connected.");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());

      delay(5000);

      //inteent connected
      //////mqttsetup();
    } else {
      Serial.println("❌ Wi-Fi connection failed.");
    }
  }
}

// void wifiloop() {
//   // Add your logic here
//   mqttloop();
// }
