/*
  KC868-A2 SOS RF Manager (Always-on RF scanning + Web API + config.json)

  Hardware:
    - RF receiver: GPIO33
    - Relay1: GPIO15

  Libraries:
    - RCSwitch
    - ArduinoJson
    - LittleFS
    - WebServer (ESP32)
    - WiFi (or merge into your existing network stack)

  APIs:
    GET    /DeviceSOSRooms.html                (serves UI if file exists in LittleFS)
    GET    /api/sos/devices                 -> { devices:[...] }
    GET    /api/sos/rf/last                 -> { seq, code, bits, proto, pulse, atMs }
    POST   /api/sos/devices/upsert          -> add/update device in config.json
    DELETE /api/sos/devices/delete?id=1     -> remove from config.json

  Config:
    /config.json includes "sos_devices":[{id,name,roomId,onCode,offCode,onBits,onProto,offBits,offProto,status,lastSeen}]
*/

#include <RCSwitch.h>

// ===================== USER SETTINGS =====================
// static const char* WIFI_SSID = "YOUR_WIFI_SSID";
// static const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// KC868-A2 pins
static const uint8_t RF_RX_PIN = 5;// 5 for SLOT 1 ,33 for Default;                  // GPIO-1 terminal = GPIO33
static const uint8_t RF_RX_PIN2 = 33;// 5 for SLOT 1 ,33 for Default;                  // GPIO-1 terminal = GPIO33

static const uint8_t RELAY1_NETWORK_STATUS_PIN = 15;  // Relay1 = GPIO15
static const uint8_t RELAY2_ALARM_LIGHT_PIN = 2;      // Relay1 = GPIO15







// Relay alarm behavior
long ALARM_MS = 10000;  // Relay ON auto-timeout
static const unsigned long DUP_IGNORE_MS = 1000;

// FS paths
static const char* CONFIG_PATH = "/config.json";
static const char* CONFIG_TMP = "/config.tmp";
static const char* UI_SOSHTML_PATH = "/DeviceSOSRooms.html";

// JSON memory (increase if you store many devices)
static const size_t CFG_SIZE = 32 * 1024;

// =========================================================

// WebServer server(80);
RCSwitch rf;

// Latest RF event (updated by loop)
volatile uint32_t g_rfCode = 0;
volatile uint16_t g_rfBits = 0;
volatile uint8_t g_rfProto = 0;
volatile uint16_t g_rfPulse = 0;
volatile uint32_t g_rfSeq = 0;
volatile uint32_t g_rfAtMs = 0;

// Duplicate filter
static uint32_t lastCode = 0;
static uint32_t lastAt = 0;

// Relay timer
static unsigned long alarmUntil = 0;

// Config document
static DynamicJsonDocument configDoc(CFG_SIZE);

bool firstLoad = true;

// ---------- Helpers ----------
static void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJson(int code, const JsonDocument& doc) {
  addCorsHeaders();
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static bool parseBodyJson(DynamicJsonDocument& in) {
  if (!server.hasArg("plain")) return false;
  return deserializeJson(in, server.arg("plain")) == DeserializationError::Ok;
}

static void ensureSosArray() {
  if (!configDoc.containsKey("sos_devices") || !configDoc["sos_devices"].is<JsonArray>()) {
    configDoc["sos_devices"] = configDoc.createNestedArray("sos_devices");
  }
}

static bool forceAllSosOffAndSave(bool resetLastSeenToZero = false) {
  // Ensure we have the latest config loaded from flash
  loadConfig();

  bool changed = false;
  JsonArray arr = sosArr();

  for (JsonObject d : arr) {
    const char* st = d["status"] | "OFF";

    // If not OFF, force OFF
    if (strcmp(st, "OFF") != 0) {
      d["status"] = "OFF";
      changed = true;
    }

    // Optional: reset lastSeen
    if (resetLastSeenToZero) {
      uint32_t ls = d["lastSeen"] | 0;
      if (ls != 0) {
        d["lastSeen"] = 0;
        changed = true;
      }
    } else {
      // Ensure the key exists
      if (!d.containsKey("lastSeen")) {
        d["lastSeen"] = 0;
        changed = true;
      }
    }

    // Ensure status key exists
    if (!d.containsKey("status")) {
      d["status"] = "OFF";
      changed = true;
    }
  }

  if (changed) {
    return saveConfigAtomic();  // saves and (in your code) publishes config to MQTT
  }

  // Nothing changed, no write
  return true;
}
static bool loadConfig() {
  ensureSosArray();
  if (configDoc.containsKey("max_siren_play")) {
    unsigned long sec = configDoc["max_siren_play"] | 10UL;  // default 10s
    ALARM_MS = sec * 1000UL;
  }
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;

  configDoc.clear();
  DeserializationError err = deserializeJson(configDoc, f);
  f.close();

  if (err) {
    // If corrupted, re-init safely
    configDoc.clear();
    configDoc.createNestedArray("sos_devices");
    return false;
  }

  ensureSosArray();
  return true;
}

static bool saveConfigAtomic() {
  File f = LittleFS.open(CONFIG_PATH, "w");  // overwrite existing
  if (!f) return false;

  size_t n = serializeJson(configDoc, f);
  f.flush();
  f.close();


  if (configDoc["mqtt_communication"] | false) {
    publishConfigToMQTT();
  }
  return (n > 0);
  // File f = LittleFS.open(CONFIG_TMP, "w");
  // if (!f) return false;

  // if (serializeJson(configDoc, f) == 0) {
  //   f.close();
  //   LittleFS.remove(CONFIG_TMP);
  //   return false;
  // }
  // f.close();

  // LittleFS.remove(CONFIG_PATH);
  // return LittleFS.rename(CONFIG_TMP, CONFIG_PATH);
}

static JsonArray sosArr() {
  ensureSosArray();
  return configDoc["sos_devices"].as<JsonArray>();
}

static int nextId() {
  int maxId = 0;
  for (JsonObject d : sosArr()) {
    int id = d["id"] | 0;
    if (id > maxId) maxId = id;
  }
  return maxId + 1;
}

static JsonObject findDeviceById(int id) {
  for (JsonObject d : sosArr()) {
    if ((int)(d["id"] | 0) == id) return d;
  }
  return JsonObject();  // invalid
}

static bool codeExistsInOtherDevice(const String& code, int currentId) {
  if (!code.length()) return false;
  for (JsonObject d : sosArr()) {
    int id = d["id"] | 0;
    if (id == currentId) continue;

    String onC = String((const char*)(d["onCode"] | ""));
    String offC = String((const char*)(d["offCode"] | ""));
    if (code == onC || code == offC) return true;
  }
  return false;
}

static void relayOn() {

  // digitalWrite(RELAY1_SIREN_PIN, HIGH);
  alarmUntil = millis() + ALARM_MS;
}

static void relayOff() {
  // digitalWrite(RELAY1_SIREN_PIN, LOW);
  alarmUntil = 0;
}

// ---------- Routes ----------
static void setupRoutes() {
  // Preflight
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      addCorsHeaders();
      server.send(204);
      return;
    }
    server.send(404, "text/plain", "Not found");
  });



  server.on("/DeviceSOSRooms", HTTP_GET, []() {
    loadingConfigFile = true;


    if (!isAuthenticated()) {
      server.sendHeader("Location", "/");
      server.send(302);
      return;
    }
    String header = readFile("/header.html");
    String form = readFile("/DeviceSOSRooms.html");

    String html;
    html.reserve(header.length() + form.length());
    html = header + form;



    html = replaceHeaderContent(html);


    server.send(200, "text/html", html);
    delay(200);
    loadingConfigFile = false;
  });

  // List devices
  server.on("/api/sos/devices", HTTP_GET, []() {
    loadConfig();
    DynamicJsonDocument out(8192);
    out["devices"] = configDoc["sos_devices"];
    sendJson(200, out);
  });

  // Latest RF event (from always-on loop)
  server.on("/api/sos/rf/last", HTTP_GET, []() {
    DynamicJsonDocument out(512);
    out["seq"] = (uint32_t)g_rfSeq;
    out["code"] = String((uint32_t)g_rfCode);
    out["bits"] = (uint16_t)g_rfBits;
    out["proto"] = (uint8_t)g_rfProto;
    out["pulse"] = (uint16_t)g_rfPulse;
    out["atMs"] = (uint32_t)g_rfAtMs;
    sendJson(200, out);
  });

  // Upsert device (add new or update existing)
  // Body:
  // {
  //   "id": 0 or existing id,
  //   "name": "Room 1 SOS",
  //   "roomId": 1,
  //   "onCode": "123", "onBits": 24, "onProto": 1,
  //   "offCode":"456", "offBits":24, "offProto":1
  // }
  server.on("/api/sos/devices/upsert", HTTP_POST, []() {
    DynamicJsonDocument in(4096);
    if (!parseBodyJson(in)) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    // Always reload from disk before modifying (protect against external edits)
    loadConfig();

    int id = in["id"] | 0;
    String name = String((const char*)(in["name"] | ""));
    int roomId = in["roomId"] | 0;

    String onCode = String((const char*)(in["onCode"] | ""));
    String offCode = String((const char*)(in["offCode"] | ""));

    int onBits = in["onBits"] | 0;
    int onProto = in["onProto"] | 0;
    int offBits = in["offBits"] | 0;
    int offProto = in["offProto"] | 0;

    // If updating, allow blank fields (keep previous)
    JsonObject d;
    if (id > 0) {
      d = findDeviceById(id);
      if (!d.isNull()) {
        // ok
      } else {
        // If id not found, treat as add
        id = 0;
      }
    }

    if (id == 0) {
      // Add new
      if (!name.length()) {
        server.send(400, "application/json", "{\"error\":\"name is required for new device\"}");
        return;
      }
      id = nextId();
      d = sosArr().createNestedObject();
      d["id"] = id;
      d["status"] = "OFF";
      d["lastSeen"] = 0;
    }

    // Duplicate protection (across other devices)
    if (onCode.length() && codeExistsInOtherDevice(onCode, id)) {
      server.send(409, "application/json", "{\"error\":\"onCode already exists\"}");
      return;
    }
    if (offCode.length() && codeExistsInOtherDevice(offCode, id)) {
      server.send(409, "application/json", "{\"error\":\"offCode already exists\"}");
      return;
    }
    if (onCode.length() && offCode.length() && onCode == offCode) {
      server.send(400, "application/json", "{\"error\":\"onCode and offCode cannot be same\"}");
      return;
    }

    // Update fields (only overwrite if provided)
    if (name.length()) d["name"] = name;
    d["roomId"] = roomId;

    if (onCode.length()) d["onCode"] = onCode;
    if (offCode.length()) d["offCode"] = offCode;

    if (in.containsKey("onBits")) d["onBits"] = onBits;
    if (in.containsKey("onProto")) d["onProto"] = onProto;
    if (in.containsKey("offBits")) d["offBits"] = offBits;
    if (in.containsKey("offProto")) d["offProto"] = offProto;

    // Ensure keys exist (optional)
    if (!d.containsKey("onCode")) d["onCode"] = "";
    if (!d.containsKey("offCode")) d["offCode"] = "";
    if (!d.containsKey("onBits")) d["onBits"] = 0;
    if (!d.containsKey("onProto")) d["onProto"] = 0;
    if (!d.containsKey("offBits")) d["offBits"] = 0;
    if (!d.containsKey("offProto")) d["offProto"] = 0;
    if (!d.containsKey("status")) d["status"] = "OFF";
    if (!d.containsKey("lastSeen")) d["lastSeen"] = 0;

    if (!saveConfigAtomic()) {
      server.send(500, "application/json", "{\"error\":\"Failed to save config\"}");
      return;
    }

    DynamicJsonDocument out(2048);
    out["ok"] = true;
    out["device"] = d;
    sendJson(200, out);
  });

  // Delete device
  server.on("/api/sos/devices/delete", HTTP_ANY, []() {
    if (server.method() != HTTP_DELETE) {
      server.send(405, "application/json", "{\"error\":\"Use DELETE\"}");
      return;
    }
    if (!server.hasArg("id")) {
      server.send(400, "application/json", "{\"error\":\"Provide id\"}");
      return;
    }

    int id = server.arg("id").toInt();
    if (id <= 0) {
      server.send(400, "application/json", "{\"error\":\"Invalid id\"}");
      return;
    }

    loadConfig();
    JsonArray arr = sosArr();

    bool removed = false;
    for (size_t i = 0; i < arr.size(); i++) {
      JsonObject d = arr[i].as<JsonObject>();
      if ((int)(d["id"] | 0) == id) {
        arr.remove(i);
        removed = true;
        break;
      }
    }

    if (!removed) {
      server.send(404, "application/json", "{\"error\":\"Not found\"}");
      return;
    }

    if (!saveConfigAtomic()) {
      server.send(500, "application/json", "{\"error\":\"Failed to save config\"}");
      return;
    }

    addCorsHeaders();
    server.send(200, "application/json", "{\"ok\":true}");
  });
}




void setupSosApiRoutes_TwoButtons() {


  // Serial.begin(115200);
  delay(200);

  // pinMode(RELAY1_SIREN_PIN, OUTPUT);
  // digitalWrite(RELAY1_SIREN_PIN, LOW);

  pinMode(RELAY1_NETWORK_STATUS_PIN, OUTPUT);
  digitalWrite(RELAY1_NETWORK_STATUS_PIN, LOW);

  delay(300);
  digitalWrite(RELAY1_NETWORK_STATUS_PIN, HIGH);

  delay(300);
  digitalWrite(RELAY1_NETWORK_STATUS_PIN, HIGH);

  pinMode(RELAY2_ALARM_LIGHT_PIN, OUTPUT);
  digitalWrite(RELAY2_ALARM_LIGHT_PIN, LOW);

  // Load config
  loadConfig();
  // If config was missing, save a new one
  if (!LittleFS.exists(CONFIG_PATH)) {
    saveConfigAtomic();
  }

  // Force all SOS statuses OFF at boot and persist
  // - pass true if you also want lastSeen = 0 for all
  forceAllSosOffAndSave(false);
  //saveConfigAtomic();


  // RF receiver always on
  rf.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
  Serial.println("RF receiver enabled on GPIO33");


  // RF receiver always on
  rf.enableReceive(digitalPinToInterrupt(RF_RX_PIN2));
  Serial.println("RF receiver enabled on GPIO3322222 A");

  

  // Routes + server
  setupRoutes();
  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loopSosDevice() {
  server.handleClient();

  // Relay auto-off
  if (alarmUntil && millis() > alarmUntil) {
    relayOff();
    Serial.println("Relay1 auto OFF (timeout)");
  }

  // RF scanning always on
  if (!rf.available()) return;

  uint32_t code = rf.getReceivedValue();
  uint16_t bits = rf.getReceivedBitlength();
  uint8_t proto = rf.getReceivedProtocol();
  uint16_t pulse = rf.getReceivedDelay();
  rf.resetAvailable();

  if (code == 0) return;

  // Duplicate ignore window
  uint32_t now = millis();
  Serial.print("Difference in Secons -----------------------------------");

  Serial.println(now - lastAt);



  if (code == lastCode && (now - lastAt) < DUP_IGNORE_MS) return;
  lastCode = code;
  lastAt = now;

  // Store latest RF event for UI polling
  g_rfCode = code;
  g_rfBits = bits;
  g_rfProto = proto;
  g_rfPulse = pulse;
  g_rfAtMs = now;
  g_rfSeq++;

  Serial.print("RF RX code=");
  Serial.print(code);
  Serial.print(" bits=");
  Serial.print(bits);
  Serial.print(" proto=");
  Serial.print(proto);
  Serial.print(" pulse=");
  Serial.println(pulse);

  // Apply to status/relay if matches known devices
  applyRfToDevicesAndRelay(code, bits, proto, pulse);

  firstLoad = false;
}


// void pushToMqtt(String code) {




//   if (config["mqtt_communication"])

//   {
//      String sosData;
//     DynamicJsonDocument sosDoc2(2048);
//     sosDoc2["serialNumber"] = config["device_serial_number"];
//     sosDoc2["roomId"] = config["roomId"];
//     sosDoc2["status"] = config["status"];
//     sosDoc2["onCode"] = config["onCode"];
//     sosDoc2["offCode"] = config["offCode"];
//     sosDoc2["type"] = "sos";
//     sosDoc2["timestamp"] = millis();
//     serializeJson(sosDoc2, sosData);
//       mqttSOSAlarmNotification(sosData);
//   } else {
//     Serial.print(" MQQT Is not enabled.");
//   }
// }




// ---------- Runtime: update status + relay from received codes ----------
static void publishSosEvent(JsonObject d, const char* newStatus) {

  if (strcmp(newStatus, "ON") == 0)
    digitalWrite(RELAY2_ALARM_LIGHT_PIN, HIGH);

  else if (strcmp(newStatus, "OFF") == 0)
    digitalWrite(RELAY2_ALARM_LIGHT_PIN, LOW);

  




  if (!configDoc.containsKey("mqtt_communication") || !(configDoc["mqtt_communication"] | false)) {
    Serial.println("MQTT disabled");
    //return;
  }

  DynamicJsonDocument payload(512);
  payload["type"] = "sos";
  payload["serialNumber"] = String((const char*)(configDoc["device_serial_number"] | ""));
  payload["id"] = d["id"] | 0;
  payload["name"] = String((const char*)(d["name"] | ""));
  payload["roomId"] = d["roomId"] | 0;
  payload["status"] = newStatus;
  payload["code"] = (newStatus[0] == 'O' && newStatus[1] == 'N') ? String((const char*)(d["onCode"] | "")) : String((const char*)(d["offCode"] | ""));
  payload["timestampMs"] = (uint32_t)millis();

  String out;
  serializeJson(payload, out);

  mqttSOSAlarmNotification(out);  // your existing publisher
  Serial.print("MQTT SOS payload: ");
  Serial.println(out);
delay(200);
  updateSOSLightStatus();
}

static void applyRfToDevicesAndRelay(uint32_t code, uint16_t bits, uint8_t proto, uint16_t pulse) {
  bool changed = false;
  JsonArray arr = sosArr();
  String codeStr = String(code);

  for (JsonObject d : arr) {
    String onC = String((const char*)(d["onCode"] | ""));
    String offC = String((const char*)(d["offCode"] | ""));

    if (onC.length() && codeStr == onC) {
      String prev = String((const char*)(d["status"] | "OFF"));
      relayOn();

      // if (prev != "ON" || firstLoad)
      {
        d["status"] = "ON";
        d["lastSeen"] = (uint32_t)millis();
        changed = true;

        publishSosEvent(d, "ON");
      }
      break;
    }

    if (offC.length() && codeStr == offC) {
      String prev = String((const char*)(d["status"] | "OFF"));
      relayOff();

      //  if (prev != "OFF" || firstLoad)
      {
        d["status"] = "OFF";
        d["lastSeen"] = (uint32_t)millis();
        changed = true;

        publishSosEvent(d, "OFF");
      }
      break;
    }
  }
  if (changed) saveConfigAtomic();
}

void updateSOSLightStatus() {

  JsonArray rooms = configDoc["sos_devices"].as<JsonArray>();

  bool anyOn = false;

  for (JsonObject room : rooms) {
    const char* status = room["status"] | "OFF";

     

    if (strcmp(status, "ON") == 0) {
      anyOn = true;
      break;  // no need to check further
    }
  }

  // Turn ON if any room alarm is ON
  // Turn OFF only if all rooms are OFF
  digitalWrite(
    RELAY2_ALARM_LIGHT_PIN,
    anyOn ? HIGH : LOW);
}
