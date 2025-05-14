#include "Arduino.h"
#include "PCF8574.h"
#include <HTTPClient.h>



#include <ArduinoJson.h>



// SIM7600 UART configuration
#define SIM7600_TX 13  // ESP32 TX2 → SIM7600 RX
#define SIM7600_RX 15  // ESP32 RX2 → SIM7600 TX

PCF8574 pcf8574_IN1(0x22, 4, 16);  // SDA, SCL

DynamicJsonDocument doc(512);

bool callActive = false;
unsigned long callStartTime = 0;
int ringCount = 0;
const int MAX_RINGS = 5;
String url = "https://alarmbackend.xtremeguard.org/api/testqueuemail";
void setup() {


  Serial.begin(115200);
  delay(1000);

  Serial2.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
  Serial.println("Initializing SIM7600...");

  pcf8574_IN1.pinMode(P0, INPUT);
  pcf8574_IN1.begin();

  // Initial AT checks
  sendATCommand("AT", 1000);
  sendATCommand("ATE0", 1000);
  sendATCommand("AT+CSQ", 1000);
  sendATCommand("AT+CREG?", 1000);

  delay(2000);
  // getFromServer(url);
  ////////sendDataToServerMail();

  //CallWifiCLient();

  // socketConnectServer();




  // Replace "internet" with your carrier's APN:
  // if (activateInternet("du")) {
  //   Serial.println("Ready for HTTP/MQTT!");
  // } else {
  //   Serial.println("Failed to connect!");
  // }
}


void CallWifiCLient() {

  String serverURL = "https://backend.xtremeguard.org/api/";
  HTTPClient http;



  doc["serialNumber"] = "XT123456";  // alarmSystem.config.deviceID;
  doc["temperature"] = 20;
  doc["humidity"] = 50;
  doc["current_mA"] = 0;
  doc["status"] = 0;
  doc["doorOpen"] = 0;
  doc["waterLeakage"] = 0;
  doc["acPowerFailure"] = 0;
  doc["fire"] = 0;
  doc["temperature_alarm"] = 0;



  String payload;
  serializeJson(doc, payload);





  http.begin(serverURL + "/alarm_device_status");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);

  doc.clear();
  //Serial.println(serverURL);

  if (httpCode > 0) {
    Serial.println("HTTP Response: " + String(httpCode));
  } else {
    Serial.println("HTTP Error: " + String(httpCode));
  }
  http.end();
}
bool socketConnectServer() {
  Serial.println("Attempting to connect to server via SIM7600E...");

  // Get server IP and port from config
  String ipString = "165.22.222.17";
  uint16_t server_port_updated = 80;  // Default port
  int temp_port = 6002;

  // Validate port range (1-65535)
  if (temp_port > 0 && temp_port <= 65535) {
    server_port_updated = (uint16_t)temp_port;
  }

  // Validate IP address
  if (ipString.length() == 0 || server_port_updated == 0) {
    Serial.println("Server IP or port is invalid.");
    delay(5000);
    return false;
  }

  Serial.print("Connecting to: ");
  Serial.print(ipString);
  Serial.print(":");
  Serial.println(server_port_updated);

  // SIM7600E TCP connection sequence
  sendATCommandCoket("AT+CIPSHUT", "OK", 5000);              // Close any existing connection
  sendATCommandCoket("AT+CIPMUX=0", "OK", 1000);             // Single connection mode
  sendATCommandCoket("AT+NETOPEN", "Network opened", 5000);  // Open network

  // Start TCP connection
  String connectCmd = "AT+CIPOPEN=0,\"TCP\",\"" + ipString + "\"," + server_port_updated;
  if (!sendATCommandCoket(connectCmd, "OK", 15000)) {  // Longer timeout for connection
    Serial.println("Failed to establish TCP connection");
    return false;
  }

  // Check connection status
  if (!sendATCommandCoket("AT+CIPSTATUS", "+CIPSTATUS: 0,3", 2000)) {  // Status 3 = connected
    Serial.println("Connection verification failed");
    return false;
  }

  Serial.println("Successfully connected to server!");


  return true;
}

void loop() {



  // Start call if button pressed
  if (isInputLowStable(P0) && !callActive) {
    Serial.println("Key1 Pressed - Initiating call...");

    // sendSMS("+971552205149", "Test message from Akil Security Alarm Systems");//working
    // delay(2000);
    startCall();  //working with Office Phone

    // sendSMS("+971552205149", "Test message from ESP32 SIM76001111111");
    // sendSMS("0552205149", "Test message from ESP32 SIM760022222222");





    // String url = "https://alarmbackend.xtremeguard.org/api/testqueuemail";
    // String postData = "temperature=29.3&humidity=70";
    // postToServer(url, postData);

    // getFromServer(url);
  }

  // Monitor call and hang up after 5 RINGs
  if (callActive) {
    monitorRings();
  }

  serialPassthrough();  // Debug passthrough

  delay(100);
}
bool sendATCommandInternet(const char* cmd, const char* expectedResponse, unsigned long timeout) {
  Serial2.println(cmd);  // Send command
  Serial.print("Sent: ");
  Serial.println(cmd);

  unsigned long startTime = millis();
  String response = "";

  while (millis() - startTime < timeout) {
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;

      // Check if expected response is found
      if (response.indexOf(expectedResponse) != -1) {
        Serial.print("Received: ");
        Serial.println(response);
        return true;
      }
    }
  }

  Serial.print("Timeout, got: ");
  Serial.println(response);
  return false;
}
void sendATCommand(String cmd, unsigned long timeout) {
  // Serial2.println(cmd);
  // Serial2.flush();

  // unsigned long startTime = millis();
  // while (millis() - startTime < timeout) {
  //   while (Serial2.available()) {
  //     Serial.write(Serial2.read());  // Optional debug
  //   }
  // }

  Serial2.println(cmd);
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (Serial2.available()) {
      String response = Serial2.readStringUntil('\n');
      response.trim();
      if (response.length()) {
        Serial.println("[SIM7600] " + response);
      }
    }
  }
}

void startCall() {
  ringCount = 0;
  Serial.println("Calling...");
  Serial2.println("ATD+971552205149;");  // Replace with actual number

  callActive = true;
  callStartTime = millis();
}

void endCall() {
  Serial.println("Hanging up after 5 rings...");
  sendATCommand("ATH", 200);  //10 seconds
  callActive = false;
}

// Check for RING messages from SIM7600
void monitorRings() {
  static String buffer = "";

  while (Serial2.available()) {
    char c = Serial2.read();
    buffer += c;

    // If end of line received, process it
    if (c == '\n') {
      buffer.trim();
      Serial.println("[SIM7600] " + buffer);

      if (buffer == "RING") {
        ringCount++;
        Serial.print("Ring Count: ");
        Serial.println(ringCount);
        if (ringCount >= MAX_RINGS) {
          endCall();
        }
      }

      // Handle NO CARRIER or BUSY
      if (buffer.indexOf("NO CARRIER") != -1 || buffer.indexOf("BUSY") != -1) {
        Serial.println("Call ended by network.");
        callActive = false;
      }

      buffer = "";  // Reset buffer
    }
  }
}

bool isInputLowStable(uint8_t pin) {
  if (pcf8574_IN1.digitalRead(pin) == LOW) {
    delay(50);
    return pcf8574_IN1.digitalRead(pin) == LOW;
  }
  return false;
}

void serialPassthrough() {
  while (Serial.available()) {
    Serial2.write(Serial.read());
  }
  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}
bool waitForResponse(String expected, unsigned long timeout) {
  unsigned long startTime = millis();
  String response = "";

  while (millis() - startTime < timeout) {
    if (Serial2.available()) {
      char c = Serial2.read();
      response += c;
      if (response.indexOf(expected) != -1) {
        return true;
      }
    }
  }
  Serial.print("Timeout waiting for: ");
  Serial.println(expected);
  return false;
}
bool sendSMSATCommand(String cmd, String expectedResponse, unsigned long timeout) {
  Serial2.println(cmd);
  return waitForResponse(expectedResponse, timeout);
}
bool sendSMS(String number, String message) {
  // 1. Set SMS Text Mode (1=text, 0=PDU)
  if (!sendSMSATCommand("AT+CMGF=1", "OK", 5000)) {
    Serial.println("Failed to set SMS text mode!");
    return false;
  }

  // 2. Set character set to GSM (optional but recommended)
  if (!sendSMSATCommand("AT+CSCS=\"GSM\"", "OK", 5000)) {
    Serial.println("Warning: Failed to set character set!");
    // Continue anyway as many modules default to GSM
  }

  // 3. Send SMS
  Serial2.print("AT+CMGS=\"");
  Serial2.print(number);
  Serial2.println("\"");

  // Wait for ">" prompt
  if (!waitForResponse(">", 5000)) {
    Serial.println("No '>' prompt received!");
    return false;
  }

  // Send message + Ctrl+Z
  Serial2.print(message);
  Serial2.write(26);  // Ctrl+Z

  // Wait for confirmation
  if (!waitForResponse("+CMGS:", 10000)) {
    Serial.println("SMS failed to send!");
    return false;
  }

  Serial.println("SMS sent successfully!");
  return true;
}
void sendSMSDu(String number, String message) {
  // 1. Set Du APN (if needed)
  sendSMSATCommand("AT+CGDCONT=1,\"IP\",\"du\"", "OK", 2000);

  // 2. Set SMS Text Mode
  if (!sendSMSATCommand("AT+CMGF=1", "OK", 2000)) {
    Serial.println("Failed to set SMS mode!");
    return;
  }

  // 3. Set SMSC (if required)
  sendSMSATCommand("AT+CSCA=\"+971501234567\"", "OK", 2000);

  // 4. Send SMS
  Serial2.print("AT+CMGS=\"");
  Serial2.print(number);
  Serial2.println("\"");

  // Wait for ">" prompt
  if (!waitForResponse(">", 5000)) {
    Serial.println("No '>' prompt!");
    return;
  }

  // Send message + Ctrl+Z
  Serial2.print(message);
  Serial2.write(26);  // Ctrl+Z

  // Wait for confirmation
  if (!waitForResponse("+CMGS:", 10000)) {
    Serial.println("SMS failed!");
  } else {
    Serial.println("SMS sent to Du UAE!");
  }
}

void postToServer(String url, String postData) {
  sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 2000);
  sendATCommand("AT+SAPBR=3,1,\"APN\",\"internet\"", 2000);  // Replace 'internet' with your SIM provider's APN
  sendATCommand("AT+SAPBR=1,1", 5000);                       // Open bearer
  sendATCommand("AT+SAPBR=2,1", 2000);                       // Query bearer

  sendATCommand("AT+HTTPINIT", 2000);
  sendATCommand("AT+HTTPPARA=\"CID\",1", 2000);
  sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", 2000);
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", 2000);

  sendATCommand("AT+HTTPDATA=" + String(postData.length()) + ",10000", 1000);
  delay(100);
  Serial2.print(postData);
  delay(1000);

  sendATCommand("AT+HTTPACTION=1", 5000);  // 1 = POST
  sendATCommand("AT+HTTPREAD", 5000);      // Read response

  sendATCommand("AT+HTTPTERM", 2000);
  sendATCommand("AT+SAPBR=0,1", 3000);  // Close bearer
}

void getFromServer(String url) {


  Serial.println("\nTesting HTTP connection...----------------------------------------------------------------------------------");

  sendATCommandResponse("AT+HTTPINIT", "OK", 2000);
  sendATCommandResponse("AT+HTTPPARA=\"CID\",1", "OK", 2000);
  sendATCommandResponse("AT+HTTPPARA=\"URL\",\"https://alarmbackend.xtremeguard.org/api/testqueuemail\"", "OK", 2000);  // Using example.com as it's more reliable for testing
  sendATCommandResponse("AT+HTTPACTION=0", "+HTTPACTION: 0,200", 5000);                                                 // Wait longer for HTTP action

  // If HTTPACTION returns 200 (success), then read
  sendATCommandResponse("AT+HTTPREAD", "+HTTPREAD:", 5000);
  sendATCommandResponse("AT+HTTPTERM", "OK", 2000);

  // Serial.println("Starting HTTP GET...");

  // sendATCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", 2000);
  // sendATCommand("AT+SAPBR=3,1,\"APN\",\"internet\"", 2000);  // Replace with your SIM APN
  // sendATCommand("AT+SAPBR=1,1", 5000);                       // Open bearer
  // sendATCommand("AT+SAPBR=2,1", 2000);                       // Query bearer

  // sendATCommand("AT+HTTPINIT", 2000);
  // sendATCommand("AT+HTTPPARA=\"CID\",1", 2000);
  // sendATCommand("AT+HTTPPARA=\"URL\",\"" + url + "\"", 2000);

  // sendATCommand("AT+HTTPACTION=0", 6000);  // 0 = GET
  // String result = readSIMResponse(6000);
  // Serial.println("HTTPACTION Response: " + result);

  // Serial2.println("AT+HTTPREAD");
  // String response = readSIMResponse(6000);
  // Serial.println("HTTPREAD Response:\n" + response);

  // sendATCommand("AT+HTTPTERM", 2000);
  // sendATCommand("AT+SAPBR=0,1", 3000);
}
String readSIMResponse(unsigned long timeout) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (Serial2.available()) {
      char c = Serial2.read();
      response += c;
    }
  }
  return response;
}

bool activateInternet(const char* apn) {

  // 1. Check if the module is responding
  if (!sendATCommandInternet("AT", "OK", 2000)) {
    Serial.println("Module not responding!");
    return false;
  }

  // 2. Set full functionality mode
  if (!sendATCommandInternet("AT+CFUN=1", "OK", 5000)) {
    Serial.println("Failed to set full functionality!");
    return false;
  }

  // 3. Prefer LTE (4G) if available (optional)
  sendATCommandInternet("AT+CNMP=38", "OK", 2000);  // LTE-only mode
  sendATCommandInternet("AT+CMNB=1", "OK", 2000);   // CAT-M (for NB-IoT/LTE-M)

  // 4. Set APN (replace "internet" with your carrier's APN)
  String cmd = "AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"";
  if (!sendATCommandInternet(cmd.c_str(), "OK", 3000)) {
    Serial.println("Failed to set APN!");
    return false;
  }

  // 5. Activate PDP context (enable data)
  if (!sendATCommandInternet("AT+CGACT=1,1", "OK", 5000)) {
    Serial.println("Failed to activate PDP context!");
    return false;
  }

  // 6. Open network (some modules need this)
  if (!sendATCommandInternet("AT+NETOPEN", "NETOPEN", 10000)) {
    Serial.println("Failed to open network!");
    return false;
  }

  // 7. Get IP address (check if connected)
  if (!sendATCommandInternet("AT+CIFSR", ".", 5000)) {  // Looks for an IP like "10.10.1.1"
    Serial.println("Failed to get IP address!");
    return false;
  }

  Serial.println("4G Internet connected!");
  return true;
}

void activateInternet2() {
  Serial.println("Activating Internet for Du...");

  sendATCommand("AT", 1000);
  sendATCommand("ATE0", 1000);        // Echo off
  sendATCommand("AT+CPIN?", 1000);    // Check SIM ready
  sendATCommand("AT+CSQ", 1000);      // Signal Quality
  sendATCommand("AT+CREG?", 1000);    // Network Registration
  sendATCommand("AT+CGATT=1", 2000);  // Attach to GPRS

  // Set APN for DU UAE
  sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 1000);
  sendATCommand("AT+SAPBR=3,1,\"APN\",\"du\"", 1000);

  // Optional: Clear bearer first
  sendATCommand("AT+SAPBR=0,1", 1000);

  // Open bearer
  sendATCommand("AT+SAPBR=1,1", 3000);

  // Query bearer
  sendATCommand("AT+SAPBR=2,1", 1000);


  delay(2000);

  sendDataToServerMail();
}

// Helper function to send AT commands and check response
bool sendATCommandCoket(String command, String expectedResponse, unsigned long timeout) {
  Serial.print("Sending: ");
  Serial.println(command);
  Serial2.println(command);

  unsigned long startTime = millis();
  String response = "";

  while (millis() - startTime < timeout) {
    if (Serial2.available()) {
      char c = Serial2.read();
      response += c;
      Serial.write(c);
    }
  }


  if (response.indexOf(expectedResponse) == -1) {
    Serial.print("ERROR: Did not receive expected response '");
    Serial.print(expectedResponse);
    Serial.println("'");
    return false;
  }
  return true;
}
void sendATCommandResponse(String command, String expectedResponse, unsigned long timeout) {
  Serial.print("Sending: ");
  Serial.println(command);
  Serial2.println(command);

  unsigned long startTime = millis();
  String response = "";

  while (millis() - startTime < timeout) {
    if (Serial2.available()) {
      char c = Serial2.read();
      response += c;
      Serial.write(c);
    }
  }

  if (response.indexOf(expectedResponse) == -1) {
    Serial.print("ERROR: Did not receive expected response");
    Serial.println(expectedResponse);
    Serial.print("Response:");
    Serial.println(response);
    Serial.println("'");
  }
}
void sendDataToServerMail() {

  String serverURL = "https://alarmbackend.xtremeguard.org/api/testqueuemail";



  Serial.println("Making HTTPS request to: " + serverURL);

  // Initialize HTTP service
  sendATCommandResponse("AT+HTTPINIT", "OK", 2000);

  // Set SSL context (may be needed for some servers)
  sendATCommandResponse("AT+HTTPSSL=1", "OK", 2000);

  // Set URL
  sendATCommandResponse("AT+HTTPPARA=\"URL\",\"" + serverURL + "\"", "OK", 2000);

  // Set content type header
  sendATCommandResponse("AT+HTTPPARA=\"USERDATA\",\"Content-Type: application/json\"", "OK", 2000);

  // Perform GET request (action 0)
  sendATCommandResponse("AT+HTTPACTION=0", "+HTTPACTION:", 10000);  // Longer timeout for HTTPS

  // Read response
  sendATCommandResponse("AT+HTTPREAD", "+HTTPREAD:", 5000);

  // Terminate HTTP service
  sendATCommandResponse("AT+HTTPTERM", "OK", 2000);

  //   sendATCommand("AT+HTTPINIT", 1000);
  // sendATCommand("AT+HTTPPARA=\"CID\",1", 1000);
  // sendATCommand("AT+HTTPPARA=\"URL\",\"https://alarmbackend.xtremeguard.org/api/testqueuemail\"", 1000);
  // sendATCommand("AT+HTTPSSL=1", 1000); // Enable HTTPS
  // sendATCommand("AT+HTTPACTION=0", 8000); // GET


  // String serverURL = "https://alarmbackend.xtremeguard.org/api/testqueuemail";
  // HTTPClient http;
  // http.begin(serverURL);
  // http.addHeader("Content-Type", "application/json");
  // int httpCode = http.GET();
  // Serial.println("HTTP Response: " + String(httpCode));
  // if (httpCode > 0) {
  //   Serial.println("HTTP Response: " + String(httpCode));
  // } else {
  //   Serial.println("HTTP Error: " + String(httpCode));
  // }

  // http.end();
}
