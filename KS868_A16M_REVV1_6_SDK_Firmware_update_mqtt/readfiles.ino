
String readConfig(String filename) {
  String path = "/" + filename;
  if (!LittleFS.exists(path)) {
    Serial.println("Config file not found: " + path);

    ensureConfigExists();
    return "Config file not found:";
  }

  File file = LittleFS.open(path, "r");
  deviceConfigContent = file.readString();
  // Serial.println("Config file found Success: ");

  file.close();

  // Serial.println(deviceConfigContent);
  deserializeJson(config, deviceConfigContent);
  // Serial.println(config["server_ip"].as<String>());



  //update Values from Device Config file to Program variables// for device.ino

  // updateDeviceSensorVariablesFromConfigFile();

  return deviceConfigContent;
}

void updateDeviceSensorVariablesFromConfigFile() {
}

// Serve static files from LittleFS
String readFile(String path) {
  File file = LittleFS.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading: " + path);
    return "no content in file : " + path;
  }
  String content = file.readString();
  file.close();
  return content;
}

// Save data to config file
// void saveConfig(String filename, String data) {
//   File file = LittleFS.open("/" + filename, FILE_WRITE);
//   if (!file) {
//     Serial.println("Failed to open file for writing: " + filename);
//     return;
//   }

//   deviceConfigContent = data;
//   file.print(data);
//   file.close();
//   Serial.println(data);
//   Serial.println("Data saved to " + filename);
// }



void saveConfig(String filename, String data) {
  // Open the config file for reading
  File file = LittleFS.open("/" + filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading: " + filename);
    return;
  }

  // Read the current content of the file into a String
  String fileContent = "";
  while (file.available()) {
    fileContent += (char)file.read();
  }
  file.close();

  // Parse the existing JSON data from the file
  DynamicJsonDocument doc(256);  // Adjust size based on the size of your JSON
  DeserializationError error = deserializeJson(doc, fileContent);

  if (error) {
    Serial.println("Failed to parse JSON from the file.");
    return;
  }

  // Parse the incoming data (new JSON string)
  DynamicJsonDocument newDoc(256);  // Adjust size based on incoming JSON
  error = deserializeJson(newDoc, data);

  if (error) {
    Serial.println("Failed to parse incoming JSON data.");
    return;
  }

  // Merge or update fields from newDoc into the existing doc
  for (JsonPair p : newDoc.as<JsonObject>()) {
    doc[p.key()] = p.value();  // Update/merge the fields
  }

  // Open the file for writing (this will overwrite the file)
  file = LittleFS.open("/" + filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + filename);
    return;
  }

  // Serialize the updated JSON back to the file
  serializeJson(doc, file);
  file.close();

  Serial.println("Updated data saved to " + filename);
}




// Function to read, update, and write back JSON data using String for filenames
void updateJsonConfig(String filename, String param, String value) {

  loadingConfigFile = true;

  if (config[param] == value) {
    loadingConfigFile = false;

    return;
  }


  // Open the file for reading
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    loadingConfigFile = false;

    return;
  }

  // Allocate a buffer for the file content
  StaticJsonDocument<126> jsonDoc;

  // Deserialize the JSON data
  DeserializationError error = deserializeJson(jsonDoc, configFile);
  if (error) {
    Serial.print("Failed to parse JSON file: ");
    Serial.println(error.c_str());
    jsonDoc.clear();  // Initialize as an empty JSON object if parsing fails
  }
  configFile.close();  // Always close after reading

  // Add or update the parameter
  // jsonDoc[param] = value;


if (param == "temperature_alerts_config") {
    // Parse JSON string
    DeserializationError error = deserializeJson(jsonDoc[param], value);
    if (error) {
      Serial.println("Failed to parse temperature_alerts_config JSON: " + String(error.c_str()));
      jsonDoc[param] = JsonArray();  // assign empty array as fallback
    }
  } else if (value == "true") {
    jsonDoc[param] = true;
  } else if (value == "false") {
    jsonDoc[param] = false;
  } else {
    jsonDoc[param] = value; // assign string as-is
  }



  // Open the file for writing (truncate the file)
  configFile = LittleFS.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    loadingConfigFile = false;

    return;
  }

  // Serialize JSON to the file
  if (serializeJson(jsonDoc, configFile) == 0) {
    Serial.println("Failed to write to file");
  }

  // Close the file
  configFile.close();

  //Serial.println("Configuration updated successfully.");

  Serial.print(param);
  Serial.print("---------------");

  Serial.println(value);
  readConfig("config.json");

  loadingConfigFile = false;
}
