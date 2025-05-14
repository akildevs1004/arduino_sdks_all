#include <ETH.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PCF8574.h>
#include <HardwareSerial.h>
#include <ModbusMaster.h>
#include <Wire.h>

// ===== Authentication =====
const char* www_username = "admin";
const char* www_password = "password123";

// ===== Network Configuration =====
#define ETH_ADDR        0
#define ETH_POWER_PIN   -1
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT

// WiFi Credentials
const char* wifi_ssid = "akil";
const char* wifi_password = "Akil1234";

// Static IP Configuration
IPAddress local_ip(192, 168, 2, 120);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 2, 1);

// ===== Hardware Config =====
#define I2C_RELAY_ADDR  0x24
#define I2C_DI_ADDR     0x22
#define I2C_SDA         4
#define I2C_SCL         16

// SIM7600E GSM Config
#define GSM_RX          13
#define GSM_TX          15
#define PWRKEY_PIN      5
HardwareSerial SIM7600E(2);

// RS485 Config
#define RS485_RX        32
#define RS485_TX        33
#define RS485_EN        14
HardwareSerial RS485Serial(1);
ModbusMaster node;

// SHT30 I2C Address
#define SHT30_I2C_ADDR  0x40

// SMS Recipient
const String PHONE_NUMBER = "+971553303991";

// ===== Global Variables =====
WebServer server(80);
PCF8574 pcf8574_RE1(I2C_RELAY_ADDR, I2C_SDA, I2C_SCL);
PCF8574 pcf8574_DI(I2C_DI_ADDR, I2C_SDA, I2C_SCL);
ModbusMaster sensor1;
ModbusMaster sensor2;
ModbusMaster sensor3;
String networkStatus = "Disconnected";
String ipAddress = "None";
bool relayStates[4] = {false};
bool diStates[8] = {false};
bool lastDI1State = false;
float temperature = 0.0;
float humidity = 0.0;

// ===== Function Prototypes =====
bool isAuthenticated();
void initEthernet();
bool initWiFi();
void powerOnSIM7600E();
void sendSMS(String message);
void readSHT30();
void initRS485();
void readRS485Sensor();
void checkDI1();
void handleRoot();
void handleRelayControl();

// ===== Authentication Helper =====
bool isAuthenticated() {
  if (!server.authenticate(www_username, www_password)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void readSensor(ModbusMaster &node, const char* name) {
  uint8_t result;
  uint16_t tempRaw, humRaw;
  float temperature, humidity;

  result = node.readHoldingRegisters(0x0000, 2);  // Temp @ 0x0000, Hum @ 0x0001

  if (result == node.ku8MBSuccess) {
    tempRaw = node.getResponseBuffer(0);
    humRaw = node.getResponseBuffer(1);

    if (tempRaw < 10000)
      temperature = tempRaw * 0.1;
    else
      temperature = -1 * (tempRaw - 10000) * 0.1;

    humidity = humRaw * 0.1;
  }
}  // This closing brace was missing



// ===== Ethernet Initialization =====
void initEthernet() {
  Serial.println("[ETH] Initializing Ethernet...");
  pinMode(ETH_POWER_PIN, OUTPUT);
  digitalWrite(ETH_POWER_PIN, LOW);
  delay(1000);
  digitalWrite(ETH_POWER_PIN, HIGH);
  delay(1000);
  ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE);
  
  unsigned long start = millis();
  while (!ETH.linkUp()) {
    if (millis() - start > 10000) {
      Serial.println("[ETH] Failed to establish link");
      networkStatus = "Disconnected";
      return;
    }
    delay(100);
  }

  if (ETH.config(local_ip, gateway, subnet, dns)) {
    networkStatus = "Ethernet";
    ipAddress = ETH.localIP().toString();
    Serial.print("[ETH] Connected. IP: ");
    Serial.println(ipAddress);
  } else {
    Serial.println("[ETH] Configuration failed");
    networkStatus = "Disconnected";
  }
}

// ===== WiFi Connection =====
bool initWiFi() {
  Serial.println("[WiFi] Connecting to WiFi...");
  WiFi.begin(wifi_ssid, wifi_password);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 15000) {
      Serial.println("[WiFi] Connection timed out");
      return false;
    }
    delay(500);
    Serial.print(".");
  }
  
  networkStatus = "WiFi";
  ipAddress = WiFi.localIP().toString();
  Serial.print("\n[WiFi] Connected. IP: ");
  Serial.println(ipAddress);
  return true;
}

// ===== SIM7600E Functions =====
void powerOnSIM7600E() {
  pinMode(PWRKEY_PIN, OUTPUT);
  digitalWrite(PWRKEY_PIN, LOW);
  delay(1000);
  digitalWrite(PWRKEY_PIN, HIGH);
  delay(2000);
  digitalWrite(PWRKEY_PIN, LOW);
  Serial.println("[GSM] SIM7600E powered on");
}

void sendSMS(String message) {
  SIM7600E.println("AT+CMGF=1");
  delay(500);
  SIM7600E.print("AT+CMGS=\"");
  SIM7600E.print(PHONE_NUMBER);
  SIM7600E.println("\"");
  delay(500);
  SIM7600E.print(message);
  delay(500);
  SIM7600E.write(26);
  Serial.println("[GSM] SMS Sent: " + message);
}

// ===== SHT30 Sensor Functions =====
void readSHT30() {
  Wire.beginTransmission(SHT30_I2C_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  Wire.endTransmission();
  delay(500);
  
  Wire.requestFrom(SHT30_I2C_ADDR, 6);
  if (Wire.available() == 6) {
    uint16_t tempRaw = (Wire.read() << 8) | Wire.read();
    Wire.read(); // CRC
    uint16_t humRaw = (Wire.read() << 8) | Wire.read();
    Wire.read(); // CRC
    
    temperature = -45 + 175 * (tempRaw / 65535.0);
    humidity = 100 * (humRaw / 65535.0);
  }
}

// ===== RS485 Sensor Functions =====
void initRS485() {
  pinMode(RS485_EN, OUTPUT);
  digitalWrite(RS485_EN, LOW);
  RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  node.begin(1, RS485Serial);
}

void readRS485Sensor() {
  digitalWrite(RS485_EN, HIGH);
  uint8_t result = node.readHoldingRegisters(0, 2);
  digitalWrite(RS485_EN, LOW);
  
  if (result == node.ku8MBSuccess) {
    temperature = node.getResponseBuffer(0) / 10.0;
    humidity = node.getResponseBuffer(1) / 10.0;
  }
}

// ===== Input Monitoring =====
void checkDI1() {
  bool currentState = (pcf8574_DI.digitalRead(0) == LOW);
  if (currentState && !lastDI1State) {
    sendSMS("ALERT: DI1 (D11) is now ACTIVE!");
  }
  lastDI1State = currentState;
}

// ===== Web Handlers =====
void handleRoot() {
  if (!isAuthenticated()) return;
  
  String page = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>Relay & DI Monitor</title>";
  page += "<style>body{font-family:Arial;margin:20px;} button{padding:10px;margin:5px;}</style>";
  page += "</head><body>";
  page += "<h1>Relay & DI Monitor</h1>";
  page += "<p><strong>Network:</strong> " + networkStatus + "</p>";
  page += "<p><strong>IP:</strong> " + ipAddress + "</p>";
  page += "<h2>Environment</h2>";
  page += "<p>Temperature: " + String(temperature) + " °C</p>";
  page += "<p>Humidity: " + String(humidity) + " %</p>";
  
  for (int i = 0; i < 4; i++) {
    page += "<p>Relay D" + String(i+1) + ": ";
    page += "<a href='/relay?num=" + String(i) + "'><button>";
    page += relayStates[i] ? "ON" : "OFF";
    page += "</button></a></p>";
  }

  page += "<h2>Digital Inputs</h2>";
  for (int i = 0; i < 8; i++) {
    page += "<p>DI" + String(i+1) + ": ";
    page += diStates[i] ? "<span style='color:red;font-weight:bold'>ACTIVE</span>" : "<span style='color:green'>INACTIVE</span>";
    page += "</p>";
  }

  page += "<script>setInterval(()=>location.reload(), 2000);</script>";
  page += "</body></html>";

  server.send(200, "text/html", page);
}

void handleRelayControl() {
  int relayNum = server.arg("num").toInt();
  if (relayNum >= 0 && relayNum < 4) {
    relayStates[relayNum] = !relayStates[relayNum];
    pcf8574_RE1.digitalWrite(relayNum, relayStates[relayNum] ? LOW : HIGH);
    server.sendHeader("Location", "/");
    server.send(303);
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  Serial.println("\nSystem starting...");

  Wire.begin(I2C_SDA, I2C_SCL);
  initRS485();

  initEthernet();
  if (networkStatus == "Disconnected") {
    if (!initWiFi()) {
      Serial.println("[NET] All network connections failed!");
    }
  }

  pcf8574_RE1.begin();
  for (int i = 0; i < 4; i++) {
    pcf8574_RE1.pinMode(i, OUTPUT);
    pcf8574_RE1.digitalWrite(i, HIGH);
  }

  pcf8574_DI.begin();
  for (int i = 0; i < 8; i++) {
    pcf8574_DI.pinMode(i, INPUT);
  }

  SIM7600E.begin(115200, SERIAL_8N1, GSM_RX, GSM_TX);
  powerOnSIM7600E();
  delay(5000);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/relay", HTTP_GET, handleRelayControl);
  server.begin();

  Serial.println("System ready.");
}

// ===== Main Loop =====
void loop() {
  server.handleClient();
  
  for (int i = 0; i < 8; i++) {
    diStates[i] = (pcf8574_DI.digitalRead(i) == LOW);
  }

  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 5000) {
    readSHT30();
    readRS485Sensor();
    lastSensorRead = millis();
  }

  checkDI1();
  delay(100);
}