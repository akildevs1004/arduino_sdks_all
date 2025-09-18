void socketVerifyConnection() {
  // Check if the client is connected
  if (!client.connected()) {
    Serial.println("Disconnected from server - Attempting to reconnect...");
    client.stop();  // Close the connection

    // Attempt to reconnect with exponential backoff
    int retryDelay = 1000;     // Start with a 1-second delay
    const int maxRetries = 1;  // Maximum number of retries

    for (int i = 0; i < maxRetries; i++) {
      if (socketConnectServer()) {
        Serial.println("Reconnected to server!");
        return;  // Exit the function after successful reconnection
      }
      delay(retryDelay);
      retryDelay *= 2;  // Double the delay for the next retry
    }

    Serial.println("Failed to reconnect to the server after multiple attempts.");
  }
}

bool socketConnectServer() {
  Serial.println("Connecting to server...");
  // Convert String to char* (C-style string)
  char server_ip_updated[server_ip.length() + 1];                    // Create a char array of proper length
  server_ip.toCharArray(server_ip_updated, server_ip.length() + 1);  // Copy the String to char*

  uint16_t server_port_updated = (uint16_t)server_port.toInt();

  // Check if the server IP is empty or the server port is zero
  if (server_ip_updated == nullptr || strlen(server_ip_updated) == 0 || server_port_updated == 0) {
    Serial.println("Server IP or port is invalid.");
    delay(5000);
    return false;
  }
  Serial.println(server_ip_updated);

  Serial.println(server_port_updated);

  if (client.connect(server_ip_updated, server_port_updated)) {
    Serial.println("Connected to server!");


    return true;
  } else {
    Serial.println("Connection to server failed.");
    return false;
  }
}

void socketDeviceHeartBeatToServer() {
  // Verify if the client is available before sending data
  socketVerifyConnection();

  if (client.connected()) {

    socketConnectionStatus = "Connected";

    updateJsonConfig("config.json", "socketConnectionStatus", "Connected");



    DynamicJsonDocument heartbeatDoc(1024);
    heartbeatDoc["serialNumber"] = serialNumber;
    heartbeatDoc["type"] = "heartbeat";
    heartbeatDoc["config"] = jsonConfig;

    String heartbeatData;
    serializeJson(heartbeatDoc, heartbeatData);

    client.println(heartbeatData);
    Serial.println("Sent heartbeat: ");

    processSocketServerRequests();
  } else {
    Serial.println("Unable to send heartbeat. Client is not connected.");
    socketConnectionStatus = "Disconnected";

    updateJsonConfig("config.json", "socketConnectionStatus", "Disconnected");
  }
}

void processSocketServerRequests() {
  // Verify if the client is available before reading data
  socketVerifyConnection();
Serial.println("Request from server:--------------------------------------- " );
  if (client.connected() && client.available()) {
    String serverRequest = client.readStringUntil('\n');
    Serial.println("Request from server:--------------------------------------- " + serverRequest);

    // Handle specific requests from the server
    if (serverRequest.indexOf("GET_CONFIG") >= 0) {
      sendResponseToServerDeviceConfiguration(serverRequest);
    }
    if (serverRequest.indexOf("UPDATE_CONFIG") >= 0) {
      updateConfigServerToDevice(serverRequest);
    }
  } else {
   Serial.println("No available data or client not connected.");
  }
}


void updateConfigServerToDevice(String message) {

  Serial.println("Received message: " + message);

  // Parse the incoming JSON message
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.println("Failed to parse JSON message");
    return;
  }

  String action = doc["action"];
  String deviceSerial = doc["serialNumber"];

  // Check if the message is meant for this device
  if (serialNumber == deviceSerial) {
    if (action == "UPDATE_CONFIG") {
      // Update the config file
      JsonObject config = doc["config"];
      ///updateConfigFile(config);

      updateJsonConfig("config.json", "serverURL", config["serverURL"]);
      updateJsonConfig("config.json", "intervalHeartbeat", config["intervalHeartbeat"]);
      updateJsonConfig("config.json", "server_ip", config["server_ip"]);
      updateJsonConfig("config.json", "server_port", config["server_port"]);
      updateJsonConfig("config.json", "gmtTimeZone", config["gmtTimeZone"]);

    }
  }

  // safeRestart();
  loadConfig();  //update from config file
  socketDeviceHeartBeatToServer();
}
void sendResponseToServerDeviceConfiguration(const String& jsonString) {
  DynamicJsonDocument doc(1024);

  // Parse the JSON string
  DeserializationError error = deserializeJson(doc, jsonString);
  if (error) {
    Serial.print("Failed to deserialize JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Access the JSON array
  JsonArray array = doc.as<JsonArray>();

  // Iterate through the JSON array and process each object
  for (const JsonObject& obj : array) {
    String request_serial_number = obj["serial_number"];
    String request_action = obj["action"];

    // Print the values
    Serial.print("serial_number: ");
    Serial.print(request_serial_number);
    Serial.print(", action: ");
    Serial.println(request_action);

    if (request_serial_number == serialNumber && request_action == "GET_CONFIG") {
      DynamicJsonDocument configDoc(1024);
      configDoc["serialNumber"] = serialNumber;
      configDoc["type"] = "config";
      configDoc["config"] = jsonConfig;

      String configData;
      serializeJson(configDoc, configData);

      if (client.connected()) {
        client.println(configData);
        Serial.println("Sent Config: " + configData);
      } else {
        Serial.println("Client not connected. Unable to send config.");
      }
    }
  }
}
