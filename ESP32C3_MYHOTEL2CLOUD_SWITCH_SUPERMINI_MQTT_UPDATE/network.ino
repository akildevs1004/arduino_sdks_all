// -------- Timers & LED profiles --------
unsigned long lastInternetCheck = 0;
// 1 hour = 60 * 60 * 1000 ms
const unsigned long CHECK_INTERVAL_OK = 60UL * 60UL * 1000UL;  // when all is good
const unsigned long CHECK_INTERVAL_DOWN = 10 * 1000;           //10UL * 1000UL;       // when offline, probe faster



static bool ledState = false;
static unsigned long lastBlinkMs = 0;


#define BUTTON_PIN 1  // use GPIO0 or any free pin


unsigned long buttonPressTime = 0;
bool buttonHeld = false;
void networkSetup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // button to GND
}

// ---- LED helpers ----
static inline void blinkLight(unsigned long interval) {



  for (int i = 0; i <= 2; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(interval);
    digitalWrite(LED_PIN, HIGH);
    delay(interval);
    digitalWrite(LED_PIN, LOW);
  }
}
static inline void handleLedIndicatorLoop() {



  if (hasInternet) isNetworkConnected = true;





  if (!isNetworkConnected) {

    digitalWrite(LED_PIN, HIGH);  //OFF - No network

    Serial.println("----------------NO NETWORK----------------------------");


  } else if (!hasInternet) {
    // Wi-Fi up, no Internet → slow blink
    blinkLight(SLOW_BLINK);
    Serial.println("----------------NO INTERNET----------------------------");

  } else {
    // All good → LED ON (change to HIGH if you want solid ON)

    isNetworkConnected = true;
    hasInternet = true;
    digitalWrite(LED_PIN, LOW);  //ON


    Serial.println("----------------YES INTERNET----------------------------");
  }
}



void networkLoop() {



  checkNetworkResetButton();

  checkNetworkConnection();
  //always
  handleLedIndicatorLoop();
}

void checkNetworkConnection() {

  const unsigned long now = millis();
  const unsigned long interval = CHECK_INTERVAL_DOWN;  //hasInternet ? CHECK_INTERVAL_OK : CHECK_INTERVAL_DOWN;

  if ((now - lastInternetCheck >= interval)) {
    lastInternetCheck = now;

    hasInternet = testInternet();
 

    
    if (!isNetworkConnected)
      connectToWiFi();
  }
}

void checkNetworkResetButton() {


  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW)
    Serial.println("checkNetworkResetButton pressed Yes ");
  // else
  //   Serial.println("checkNetworkResetButton pressed No");




  if (buttonState == LOW) {  // pressed

    if (buttonPressTime == 0) {
      buttonPressTime = millis();  // first press time
    } else
      Serial.print("buttonHeld Continuous  ");


    if (buttonHeld)
      Serial.println(millis() - buttonPressTime);



    if (!buttonHeld && (millis() - buttonPressTime >= 5000)) {

      Serial.println("RESET Button Activates and Resetting Wifi Network ");
      buttonHeld = true;
      resetNetworkAPMode();  // call your method
    }
  } else {  // released
    buttonPressTime = 0;
    buttonHeld = false;
  }  // wait 0.5 sec
}

void resetNetworkAPMode() {
  // updateJsonConfig("config.json", "wifiSSID", wifiSSID);
  // wifiPassword = "";
  wifiPassword = "";
  updateJsonConfig("config.json", "wifiPassword", "");

  Serial.println("🔄 Resetting Wi-Fi...");
  // 1) Hard disconnect and erase stored creds

  WiFi.persistent(true);
  // Disconnect from any STA (client) connection
  WiFi.disconnect(true, true);  // true,true → erase old config too
  delay(1000);

  ESP.restart();
}

// ---- Wi-Fi events (NON-blocking; no delays; don't call networkLoop here) ----
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_READY:
      Serial.println("📶 Wi-Fi Ready");
      WiFi.disconnect(false, false);  // keep config, drop connection
      delay(50);
      WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
      isNetworkConnected = false;
      hasInternet = false;
      connectToWiFi();
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("✅ Wi-Fi Connected (AP)");
      isNetworkConnected = true;
      hasInternet = false;

      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("📶 Wi-Fi IP: ");
      Serial.println(WiFi.localIP());
      isNetworkConnected = true;
      hasInternet = false;  // next networkLoop() will test & set
      networkLoop();
      mqttsetup();  // safe to start MQTT now that we have IP
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("❌ Wi-Fi Disconnected");
      isNetworkConnected = false;
      hasInternet = false;
      networkLoop();


      break;

    default:
      Serial.println("ℹ️ Other Wi-Fi event");
      /////////networkLoop();
      break;
  }
}


bool testInternet() {
  uint16_t timeoutMs = 5000;
  // if (WiFi.status() != WL_CONNECTED) {
  //   Serial.println("❌ WiFi not connected");
  //   hasInternet = false;

  //   return false;
  // }

  HTTPClient http;
  http.setTimeout(timeoutMs);
  const char* probe = "http://clients3.google.com/generate_204";
  if (!http.begin(probe)) {
    Serial.println("❌ HTTP begin() failed");
    hasInternet = false;
    return false;
  }
  int code = http.GET();  // should be 204 if OK
  http.end();

  bool httpOK = (code == 204);
  Serial.printf("%s HTTP 204 check [%s] -> code: %d\n",
                httpOK ? "✅" : "❌", probe, code);


  hasInternet = httpOK;
  return httpOK;
}

unsigned long previousHeartbeatMillisSocket = 0;  // Stores last time heartbeat was sent




unsigned long previousHeartbeatMillis = 0;  // Stores last time heartbeat was sent
void handleHeartbeat() {

  // Serial.print("Heartbeat ");
  // Serial.println(config["heartbeat"].as<int>());

  intervalHeartbeat = jsonConfig["intervalHeartbeat"].as<int>();

  if (intervalHeartbeat <= 30) {
    intervalHeartbeat = 60;
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousHeartbeatMillis >= intervalHeartbeat * 1000) {
    previousHeartbeatMillis = currentMillis;

    String heartbeatData;
    DynamicJsonDocument heartbeatDoc2(2048);
    heartbeatDoc2["serialNumber"] = jsonConfig["SerialNumber"];
    heartbeatDoc2["type"] = "heartbeat";
    heartbeatDoc2["timestamp"] = millis();
    //heartbeatDoc2["config"] = readConfig("config.json");  //deviceConfigContent;  // ////////readConfig("config.json");
    //heartbeatDoc["sensor_data"] = sensorData;      // ////////readConfig("config.json");
    serializeJson(heartbeatDoc2, heartbeatData);
    // hasInternet = testInternet();
    // if (!isNetworkConnected  ) {
    //   connectToWiFi();
    // }

    if (hasInternet)
      mqttHeartBeat(heartbeatData);
  }
}
