
// Save configuration callback
void saveConfigCallback() {
  shouldSaveConfig = true;
  Serial.println("Config will be saved.");

  // ESP.restart();   // full software reset
}

// List files in LittleFS
void listLittleFSFiles() {
  File root = LittleFS.open("/");
  if (!root.isDirectory()) {
    Serial.println("Error: Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    Serial.print(file.name());
    Serial.println(file.isDirectory() ? " [DIR]" : " [FILE]");
    file = root.openNextFile();
  }
}

// Load configuration from LittleFS
bool loadConfig() {
  File configFile = LittleFS.open("/config.json", FILE_READ);
  if (!configFile) {
    Serial.println("No config file found.");
    return false;
  }

  // DynamicJsonDocument json(1024);
  DeserializationError error = deserializeJson(jsonConfig, configFile);
  configFile.close();

  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }

  serverURL = jsonConfig["serverURL"].as<String>();
  serialNumber = jsonConfig["SerialNumber"].as<String>();
  device_serial_number = jsonConfig["SerialNumber"].as<String>();
  ipAddress = jsonConfig["ipAddress"].as<String>();

  wifiSSID = jsonConfig["wifiSSID"].as<String>();
  wifiPassword = jsonConfig["wifiPassword"].as<String>();
  //temperatureThreshold = jsonConfig["temperatureThreshold"].as<String>();
  server_ip = jsonConfig["server_ip"].as<String>();
  server_port = jsonConfig["server_port"].as<String>();
  intervalHeartbeat = jsonConfig["intervalHeartbeat"].as<int>();
  //if (intervalHeartbeat <= 0) intervalHeartbeat = 10*1000;
  ipAddress = jsonConfig["ipAddress"].as<String>();
  mqtt_server = jsonConfig["mqtt_server"].as<String>();
  mqtt_port = jsonConfig["mqtt_port"].as<int>();
  mqtt_clientId = jsonConfig["mqtt_clientId"].as<String>();
  PASSWORD = jsonConfig["password"].as<String>();



  return true;
}

// Save configuration to LittleFS
bool saveConfig() {
  File configFile = LittleFS.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  DynamicJsonDocument jsonEdit(1024);
  jsonEdit["serverURL"] = serverURL;
  jsonEdit["SerialNumber"] = serialNumber;
  jsonEdit["ipAddress"] = ipAddress;

  jsonEdit["mqtt_server"] = mqtt_server;
  jsonEdit["mqtt_port"] = mqtt_port;
  jsonEdit["mqtt_clientId"] = mqtt_clientId;

  jsonEdit["password"] = PASSWORD;


  jsonEdit["wifiSSID"] = wifiSSID;
  jsonEdit["wifiPassword"] = wifiPassword;
  jsonEdit["intervalHeartbeat"] = intervalHeartbeat;
  jsonEdit["server_ip"] = server_ip;
  jsonEdit["server_port"] = server_port;
  //jsonEdit["gmtTimeZone"] = gmtTimeZone;
  jsonEdit["socketConnectionStatus"] = socketConnectionStatus;

  if (serializeJson(jsonEdit, configFile) == 0) {
    Serial.println("Failed to write to config file");
    configFile.close();
    return false;
  }

  configFile.close();

  // safeRestart();
  loadConfig();  //update from config file
  return true;
}

void safeRestart() {
  Serial.println("Restarting device...");
  ESP.restart();
}

// Function to read, update, and write back JSON data
// Function to read, update, and write back JSON data using String for filenames
void updateJsonConfig(const String& filename, const char* param, const char* value) {
  // if (!LittleFS.begin()) {
  //   Serial.println("Failed to mount LittleFS");
  //   return;
  // }

  // // Convert String to const char* using .c_str()
  // const char* filePath = filename.c_str();

  // // Check if the file exists, create an empty file if it doesn't exist
  // if (!LittleFS.exists(filePath)) {
  //   File configFile = LittleFS.open(filePath, FILE_WRITE);
  //   if (!configFile) {
  //     Serial.print("Failed to create file: ");
  //     Serial.println(filePath);
  //     return;
  //   }
  //   configFile.close();
  // }

  // Open the file for reading
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    return;
  }

  // Allocate a buffer for the file content
  StaticJsonDocument<512> jsonDoc;

  // Deserialize the JSON data
  DeserializationError error = deserializeJson(jsonDoc, configFile);
  if (error) {
    Serial.print("Failed to parse JSON file: ");
    Serial.println(error.c_str());
    jsonDoc.clear();  // Initialize as an empty JSON object if parsing fails
  }
  configFile.close();  // Always close after reading

  // Add or update the parameter
  jsonDoc[param] = value;

  jsonConfig[param] = value;

  // Open the file for writing (truncate the file)
  configFile = LittleFS.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  // Serialize JSON to the file
  if (serializeJson(jsonDoc, configFile) == 0) {
    Serial.println("Failed to write to file");
  }

  // Close the file
  configFile.close();


  Serial.println("Configuration updated successfully.- ");

  Serial.print(param);

  loadConfig();  // Update the runtime configuration from the updated file
}

// Function to find parameter value in JSON
String findParamValue(const char* paramKey) {


  // Access the value of the specified parameter key
  const char* paramValue = jsonConfig[paramKey];
  if (paramValue) {
    return String(paramValue);  // Return the found value as a String
  } else {
    Serial.println("Parameter not found.");
    return "";  // Return an empty string if parameter is not found
  }
}

void updateConfigFile(JsonObject config) {
  // Open the config file for writing (overwrite mode)
  File file = LittleFS.open("/config.json", FILE_WRITE);

  if (!file) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  // Serialize the new config to the file
  serializeJson(config, file);
  file.close();

  Serial.println("Config file updated");

  // Optionally read back the file to verify
  File readFile = LittleFS.open("/config.json", FILE_READ);
  if (readFile) {
    Serial.println("Updated Config File Content:");
    while (readFile.available()) {
      Serial.write(readFile.read());
    }
    readFile.close();
  }
}
