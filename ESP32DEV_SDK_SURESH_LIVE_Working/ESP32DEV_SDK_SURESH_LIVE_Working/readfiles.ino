
String readConfig(String filename) {
  String path = "/" + filename;
  if (!LittleFS.exists(path)) {
    Serial.println("Config file not found: " + path);
    return "Config file not found:";
  }

  File file = LittleFS.open(path, "r");
  deviceConfigContent = file.readString();
  Serial.println("Config file found Success: ");

  file.close();

  deserializeJson(config, deviceConfigContent);
  //update Values from Device Config file to Program variables// for device.ino

 
  if (config.containsKey("max_temperature")) {
    TEMPERATURE_THRESHOLD = config["max_temperature"].as<double>();
  }
  // if (config.containsKey("max_humidity")) {
  //   HUMIDIY_THRESHOLD = config["max_humidity"].as<double>();
  // }
  if (config.containsKey("server_url")) {
    serverURL = config["server_url"].as<String>();
  }

  // if (config.containsKey("temp_checkbox")) {
  //   temp_checkbox = config["temp_checkbox"].as<bool>();
  // }
  // if (config.containsKey("humidity_checkbox")) {
  //   humidity_checkbox = config["humidity_checkbox"].as<bool>();
  // }
  // if (config.containsKey("water_checkbox")) {
  //   water_checkbox = config["water_checkbox"].as<bool>();
  // }
  // if (config.containsKey("fire_checkbox")) {
  //   fire_checkbox = config["fire_checkbox"].as<bool>();
  // }
  // if (config.containsKey("power_checkbox")) {
  //   power_checkbox = config["power_checkbox"].as<bool>();
  // }
  // if (config.containsKey("door_checkbox")) {
  //   door_checkbox = config["door_checkbox"].as<bool>();
  // }
  // if (config.containsKey("siren_checkbox")) {
  //   siren_checkbox = config["siren_checkbox"].as<bool>();
  // }


  // if (door_checkbox == true) {
  //   doorCountdownDuration = config["max_doorcontact"].as<long>();
  // }












  return deviceConfigContent;
}

// Serve static files from LittleFS
String readFile(String path) {
  File file = LittleFS.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading: " + path);
    return "no content in file : "+path;
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
  DynamicJsonDocument doc(1024);  // Adjust size based on the size of your JSON
  DeserializationError error = deserializeJson(doc, fileContent);

  if (error) {
    Serial.println("Failed to parse JSON from the file.");
    return;
  }

  // Parse the incoming data (new JSON string)
  DynamicJsonDocument newDoc(1024);  // Adjust size based on incoming JSON
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
  // jsonDoc[param] = value;


  if (value == "true") {
    jsonDoc[param] = true;
  } else if (value == "false") {
    jsonDoc[param] = false;
  } else {
    jsonDoc[param] = value;  // Assign as is if it's neither "true" nor "false"
  }




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

  //Serial.println("Configuration updated successfully.");

  Serial.println(param);
  readConfig("config.json");
}
