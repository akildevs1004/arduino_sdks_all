

unsigned long lastInternetCheck = 0;
const unsigned long checkInterval = 30 * 60 * 1000;  // 1 hour = 60*60*1000 ms

void networkLoop() {


  unsigned long nowInternet = millis();

  if (nowInternet - lastInternetCheck >= checkInterval) {
    lastInternetCheck = nowInternet;

    bool hasInternet = testInternet();

    if (!hasInternet || jsonConfig["mqtt"] == "offline") {
      Serial.println("🔁 Restarting due to no internet...");
      delay(2000);
      ESP.restart();
    }
  }
}



unsigned long previousHeartbeatMillisSocket = 0;  // Stores last time heartbeat was sent

 

  
unsigned long previousHeartbeatMillis = 0;  // Stores last time heartbeat was sent
void handleHeartbeat() {

  // Serial.print("Heartbeat ");
  // Serial.println(config["heartbeat"].as<int>());

  intervalHeartbeat = jsonConfig["intervalHeartbeat"].as<int>();

  if (intervalHeartbeat <= 5) {
    intervalHeartbeat = 20;
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousHeartbeatMillis >= intervalHeartbeat * 1000) {
    previousHeartbeatMillis = currentMillis;
  blinkBlueLight(2);
    String heartbeatData;
    DynamicJsonDocument heartbeatDoc2(2048);
    heartbeatDoc2["serialNumber"] = jsonConfig["SerialNumber"];
    heartbeatDoc2["type"] = "heartbeat";
    heartbeatDoc2["timestamp"] = millis();
    //heartbeatDoc2["config"] = readConfig("config.json");  //deviceConfigContent;  // ////////readConfig("config.json");
    //heartbeatDoc["sensor_data"] = sensorData;      // ////////readConfig("config.json");
    serializeJson(heartbeatDoc2, heartbeatData);
    mqttHeartBeat(heartbeatData);
  }
}