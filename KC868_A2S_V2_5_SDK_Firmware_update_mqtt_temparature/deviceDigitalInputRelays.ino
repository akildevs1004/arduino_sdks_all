// ======================================================
// SIMPLE 2 INPUT + 2 RELAY SYSTEM (ESP32)
// ======================================================

// -------------------- DIGITAL INPUTS --------------------
#define DI1_PIN 36  // Water sensor
#define DI2_PIN 39  // Fire sensor



// -------------------- RELAYS --------------------
#define RELAY1_PIN 15  // FAN
#define RELAY2_PIN 2   // SIREN


// -------------------- RELAYS --------------------
#define RELAY_GATE0 15  // FAN
#define RELAY_GATE1 2   // SIREN



// ================== SETTINGS ==================
// Change to false if your sensor is ACTIVE LOW
#define SENSOR_ACTIVE_HIGH true

// Change to false if your relay turns ON when HIGH
#define RELAY_ACTIVE_LOW true


// -------------------- FACTORY RESET BUTTON (BOOT) --------------------
#define RESET_PIN 0         // ✅ your black button
#define RESET_HOLD_MS 5000  // hold 5 seconds
#define RESET_DEBOUNCE_MS 30
#define RESET_ACTIVE_LEVEL LOW  // pressed = LOW


JsonObject alarmObj;
JsonArray alarms;
struct AlarmPinMap {
  int id;
  int pin;
};
AlarmPinMap ALARM_PINS[] = {
  { 1, DI1_PIN },  // water_alarm
  { 2, DI2_PIN },  // door_alarm (or fire_alarm)
};
const int ALARM_PINS_COUNT = sizeof(ALARM_PINS) / sizeof(ALARM_PINS[0]);
unsigned long alarmTriggerTime[ALARM_PINS_COUNT] = { 0 };  // adjust size to max alarms




int lastSentStatus[ALARM_PINS_COUNT];

// ======================================================
bool temperature_alarm = false;
bool isAAnylarmOn = false;
void digitalINPUTsSetup() {
  Serial.begin(115200);

  // Setup inputs
  pinMode(DI1_PIN, INPUT);
  pinMode(DI2_PIN, INPUT);

  // Setup relays
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  // BOOT button as input with internal pull-up
  pinMode(RESET_PIN, INPUT_PULLUP);

  // Turn OFF relays initially
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);
  } else {
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
  }

  Serial.println("System Ready...");


  //Second step
  attachPinsToConfig();
  initAlarmSendCache();
}

// Helper function to detect active state
bool isSensorActive(int pin) {

  return digitalRead(pin) == HIGH ? true : false;

  // if (SENSOR_ACTIVE_HIGH)
  //   return digitalRead(pin) == HIGH;
  // else
  //   return digitalRead(pin) == LOW;
}

// Relay control helper
void setRelay(int pin, bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? HIGH : LOW);
  } else {
    digitalWrite(pin, on ? LOW : HIGH);
  }
}

void DitialInputloop() {

  verifyAlarm();


  checkLongPressReset();
  delay(500);
}

void verifyAlarm() {

  bool anyAlarmActive = false;
  JsonArray alarms = config["alarms"].as<JsonArray>();

  for (int i = 0; i < (int)alarms.size() && i < ALARM_PINS_COUNT; i++) {

    JsonObject alarmObj = alarms[i];

    int pin = alarmObj["pin"] | -1;
    int delaySec = alarmObj["delay"] | 0;
    bool enabled = alarmObj["checkbox_enabled"] | false;  // ✅ correct for true/false

    const char* name = alarmObj["alarm_name"] | "unknown";

    bool rawActive = (enabled && pin >= 0) ? isSensorActive(pin) : false;

    if(!rawActive)delaySec=0;

    bool active = false;

    if (rawActive) {
      if (delaySec == 0) {
        active = true;
      } else {
        if (alarmTriggerTime[i] == 0) alarmTriggerTime[i] = millis();
        if (millis() - alarmTriggerTime[i] >= (delaySec * 1000UL)) active = true;
      }
    } else {
      alarmTriggerTime[i] = 0;
    }

    alarmObj["alarm_status"] = active;

    // Serial.printf("Alarm[%d] %s | enabled=%d | pin=%d | delay=%ds | status=%s\n",
    //               i, name, enabled, pin, delaySec, active ? "ON" : "OFF");

    if (active) anyAlarmActive = true;

    // ✅ send only on change
    int curr = active ? 1 : 0;
    if (lastSentStatus[i] != curr) {
      lastSentStatus[i] = curr;

      StaticJsonDocument<256> doc;
      String payload;

      doc["serialNumber"] = device_serial_number;
      doc["type"] = "alarm";
      doc["digital_input_number"] = pin;
      doc["timestamp"] = millis();
      doc[name] = curr;  // ✅ dynamic key name

      serializeJson(doc, payload);
      sendTemperatureDataToServerHttp(payload);
    }
  }

  setRelay(RELAY2_PIN, anyAlarmActive);
}

//------------------------
void initAlarmSendCache() {
  for (int i = 0; i < ALARM_PINS_COUNT; i++) lastSentStatus[i] = -1;
}
void attachPinsToConfig() {
  alarms = config["alarms"].as<JsonArray>();

  for (JsonObject a : alarms) {
    int id = a["id"] | 0;
    int pin = pinForAlarmId(id);

    a["pin"] = pin;                     // ✅ additional key
    if (pin >= 0) pinMode(pin, INPUT);  // GPIO36/39 INPUT only
  }




  File file = LittleFS.open("/config.json", FILE_WRITE);
  serializeJsonPretty(config, file);
  file.close();
}
int pinForAlarmId(int id) {
  for (int i = 0; i < ALARM_PINS_COUNT; i++) {



    if (ALARM_PINS[i].id == id) {

      Serial.print("Map ID: ");
      Serial.println(ALARM_PINS[i].id);

      Serial.print("Config ID: ");
      Serial.println(id);

      Serial.print("Return Mapped Pin: ");
      Serial.println(ALARM_PINS[i].pin);

      return ALARM_PINS[i].pin;
    }
  }
  return -1;  // not mapped
}
void relaysSetup() {
}
void relayLoop() {
}
void updateRelayStatusAction(int relayNum, bool status) {
}
void callRelayBuzzerTurn(bool buzzerShouldBeOn) {
}

void updateRelayStatus(int relayNum) {
}
void handleRelayControl() {
}
bool checkAnyAlarmOpen() {
  return false;
}






// Long press detection (non-blocking)
void checkLongPressReset() {
  static int lastStable = HIGH;
  static int lastRead = HIGH;
  static unsigned long lastDebounceAt = 0;

  static bool pressing = false;
  static unsigned long pressStartedAt = 0;

  int reading = digitalRead(RESET_PIN);

  // debounce
  if (reading != lastRead) {
    lastDebounceAt = millis();
    lastRead = reading;
  }

  if ((millis() - lastDebounceAt) > RESET_DEBOUNCE_MS) {
    if (reading != lastStable) {
      lastStable = reading;

      if (lastStable == RESET_ACTIVE_LEVEL) {
        pressing = true;
        pressStartedAt = millis();
        Serial.println("⏳ BOOT button pressed... keep holding 5s to factory reset");
      } else {
        pressing = false;
      }
    }
  }

  if (pressing && (millis() - pressStartedAt >= RESET_HOLD_MS)) {
    Serial.println("\n⚠ FACTORY RESET: clearing saved config...");
    Serial.println("✅ Cleared. Restarting...");
    restoreDefaultConfig();
    
  }
}
