
//Socket - heartbeat
unsigned long previousHeartbeatMillis = 0;        // Stores last time heartbeat was sent
unsigned long previousHeartbeatMillisSocket = 0;  // Stores last time heartbeat was sent

long intervalHeartbeat = 10;  // Interval at which to send heartbeat (10 seconds)


String socketConnectionStatus = "Disconencted";


//socket settings end
void socketVerifyConnection() {
  // Check if the client is connected
  if (!client.connected()) {
    Serial.println("Disconnected from server - Attempting to reconnect...");
    client.stop();  // Close the connection

    // Attempt to reconnect with exponential backoff
    int retryDelay = 1000;     // Start with a 1-second delay
    const int maxRetries = 5;  // Maximum number of retries

    for (int i = 0; i < maxRetries; i++) {
      if (socketConnectServer()) {
        Serial.println("Reconnected to server!");
        return;  // Exit the function after successful reconnection
      }
      delay(retryDelay);
      retryDelay *= 2;  // Double the delay for the next retry
    }

    Serial.println("Failed to reconnect to the server after multiple attempts.");
  } else {
    ///////Serial.println("Socket Connection is available");
  }
}

bool socketConnectServer() {
  Serial.println("Connecting to server...");
  // Convert String to char* (C-style string)
  char server_ip_updated[100];  // Create a char array of proper length
  // config["server_ip"].toCharArray(server_ip_updated, 100);  // Copy the String to char*

  String ipString = config["server_ip"].as<String>();


  Serial.println("Config Server IP:" + ipString);


  if (ipString.length() < sizeof(server_ip_updated)) {
    ipString.toCharArray(server_ip_updated, sizeof(server_ip_updated));
  }

  // uint16_t server_port_updated = (uint16_t)config["server_port"].toInt();

  uint16_t server_port_updated = 80;  // Default port

  int temp_port = config["server_port"].as<int>();

  // Validate port range (1-65535)
  if (temp_port > 0 && temp_port <= 65535) {
    server_port_updated = (uint16_t)temp_port;
  }


  // Check if the server IP is empty or the server port is zero
  if (server_ip_updated == nullptr || strlen(server_ip_updated) == 0 || server_port_updated == 0) {
    Serial.println("Server IP or port is invalid.");
    delay(5000);
    return false;
  }

  Serial.print("server_ip_updated-");

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

    //updateJsonConfig("config.json", "socketConnectionStatus", "Connected");



    DynamicJsonDocument heartbeatDoc(1024);
    heartbeatDoc["serialNumber"] = config["device_serial_number"];
    heartbeatDoc["type"] = "heartbeat";
    heartbeatDoc["config"] = deviceConfigContent;  // ////////readConfig("config.json");
    heartbeatDoc["sensor_data"] = sensorData;  // ////////readConfig("config.json");    

    String heartbeatData;
    serializeJson(heartbeatDoc, heartbeatData);

    client.println(heartbeatData);
    //Serial.println("Sent heartbeat: ");


  } else {
    Serial.println("Unable to send heartbeat. Client is not connected.");
    socketConnectionStatus = "Disconnected";

    updateJsonConfig("config.json", "socketConnectionStatus", "Disconnected");
  }
}

void processSocketServerRequests() {
  
  ///////////////////socketVerifyConnection();
  //Serial.println("Checking Request from server:--------------------------------------- ");
  if (client.connected() && client.available()) {
    String serverRequest = client.readStringUntil('\n');
    //Serial.println("Request from server:--------------------------------------- " + serverRequest);

    // Handle specific requests from the cloud
    if (serverRequest.indexOf("GET_CONFIG") >= 0) {
      sendResponseToServerDeviceConfiguration(serverRequest);
    }
    if (serverRequest.indexOf("UPDATE_CONFIG") >= 0) {
      updateConfigServerToDevice(serverRequest);
      //Serial.println("--------------------------RESTARTING DEVICE--------------------------------- ");
      socketDeviceHeartBeatToServer();
      /////// handleRestartDevice();  // Restart device to effect the new cloud settings
    }
  } else {
    /////////Serial.println("No available data or client not connected.");
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
  if (device_serial_number == deviceSerial) {
    if (action == "UPDATE_CONFIG") {
      // Update the config file
      JsonObject configCloudServer = doc["config"];
    

      for (JsonPair kv : configCloudServer) {
        const char* key = kv.key().c_str();  // Get the key
        JsonVariant value = kv.value();      // Get the value

        // Do something with the key-value pair
        Serial.print("Key: ");
        Serial.print(key);
        Serial.print(", Value: ");
        Serial.println(value.as<String>());
        updateJsonConfig("config.json", key, value);
      }
    }
  }

  // safeRestart();
  // loadConfig();  //update from config file
  readConfig("config.json");
  socketDeviceHeartBeatToServer();
  handleRestartDevice();
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

    if (request_serial_number == device_serial_number && request_action == "GET_CONFIG") {
      DynamicJsonDocument configDoc(1024);
      configDoc["serialNumber"] = device_serial_number;
      configDoc["type"] = "config";
      configDoc["config"] = deviceConfigContent;  //readConfig("config.json");
    configDoc["sensor_data"] = sensorData;  //  


      String configData;
      serializeJson(configDoc, configData);

      if (client.connected()) {
        client.println(configData);
        Serial.println("Sent Config to SDK: " + configData);
      } else {
        Serial.println("Client not connected. Unable to send config.");
      }
    }
  }
}

void handleHeartbeat() {

  // Serial.print("Heartbeat ");
  // Serial.println(config["heartbeat"].as<int>());

  int heartBeatSeconds = 10;
  if (config["heartbeat"].as<int>() > 10) {
    heartBeatSeconds = 10;
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousHeartbeatMillis >= 5 * 1000) {
    previousHeartbeatMillis = currentMillis;
    socketDeviceHeartBeatToServer();
  }
  

  unsigned long currentMillisSocket = millis();

  if (currentMillisSocket - previousHeartbeatMillisSocket >= 1 * 1000) {
    previousHeartbeatMillisSocket = currentMillisSocket;
    processSocketServerRequests();
  }
}
 