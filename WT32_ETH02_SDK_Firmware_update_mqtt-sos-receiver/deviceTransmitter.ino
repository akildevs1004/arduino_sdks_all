/*
  KC868-A2 SOS RF Manager (Always-on RF scanning + Web API + config.json)

  Function rule:
    - onCode       => status becomes "ON"
    - threadOnCode => status also becomes "ON"   (only internal source is different)
    - offCode      => status becomes "OFF"

  Config structure:
    "sos_devices":[
      {
        "id":1,
        "name":"Room 1 SOS",
        "roomId":1,
        "onCode":"123",
        "offCode":"456",
        "threadOnCode":"789",
        "onBits":24,
        "onProto":1,
        "offBits":24,
        "offProto":1,
        "threadOnBits":24,
        "threadOnProto":1,
        "status":"OFF",
        "lastSeen":0
      }
    ]
*/

#include <RCSwitch.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// ===================== USER SETTINGS =====================
static const uint8_t RF_RX_PIN = 2;
static const uint8_t RF_RX_PIN2 = 4;

static const uint8_t RELAY1_NETWORK_STATUS_PIN = 15;
static const uint8_t RELAY2_ALARM_LIGHT_PIN = 2;

// Relay alarm behavior
long ALARM_MS = 10000;
static const unsigned long DUP_IGNORE_MS = 1000;

// FS paths
static const char* CONFIG_PATH = "/config.json";
static const char* CONFIG_TMP = "/config.tmp";
static const char* UI_SOSHTML_PATH = "/DeviceSOSRooms.html";
static const char* UI_SOSJS_PATH = "/DeviceSOSRooms.js";

// JSON memory
static const size_t CFG_SIZE = 32 * 1024;

// =========================================================

// Expected from main project
// extern WebServer server;
// extern bool loadingConfigFile;
// bool isAuthenticated();
// String readFile(const char* path);
// String replaceHeaderContent(const String& html);
// void mqttSOSAlarmNotification(const String& payload);

RCSwitch rf;

// Latest RF event
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

// Config
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

static JsonArray sosArr() {
  ensureSosArray();
  return configDoc["sos_devices"].as<JsonArray>();
}

static bool loadConfig() {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    configDoc.clear();
    configDoc.createNestedArray("sos_devices");
    return false;
  }

  configDoc.clear();
  DeserializationError err = deserializeJson(configDoc, f);
  f.close();

  if (err) {
    configDoc.clear();
    configDoc.createNestedArray("sos_devices");
    return false;
  }

  ensureSosArray();

  if (configDoc.containsKey("max_siren_play")) {
    unsigned long sec = configDoc["max_siren_play"] | 10UL;
    ALARM_MS = sec * 1000UL;
  }

  return true;
}

static bool saveConfigAtomic() {
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;

  size_t n = serializeJson(configDoc, f);
  f.flush();
  f.close();

  if (configDoc["mqtt_communication"] | false) {
    // publishConfigToMQTT();
  }

  return (n > 0);
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
  return JsonObject();
}

static bool codeExistsInOtherDevice(const String& code, int currentId) {
  if (!code.length()) return false;

  for (JsonObject d : sosArr()) {
    int id = d["id"] | 0;
    if (id == currentId) continue;

    String onC = String((const char*)(d["onCode"] | ""));
    String offC = String((const char*)(d["offCode"] | ""));
    String threadOnC = String((const char*)(d["threadOnCode"] | ""));

    if (code == onC || code == offC || code == threadOnC) return true;
  }
  return false;
}

static bool forceAllSosOffAndSave(bool resetLastSeenToZero = false) {
  loadConfig();

  bool changed = false;
  JsonArray arr = sosArr();

  for (JsonObject d : arr) {
    const char* st = d["status"] | "OFF";

    if (strcmp(st, "OFF") != 0) {
      d["status"] = "OFF";
      changed = true;
    }

    if (resetLastSeenToZero) {
      uint32_t ls = d["lastSeen"] | 0;
      if (ls != 0) {
        d["lastSeen"] = 0;
        changed = true;
      }
    } else {
      if (!d.containsKey("lastSeen")) {
        d["lastSeen"] = 0;
        changed = true;
      }
    }

    if (!d.containsKey("status")) {
      d["status"] = "OFF";
      changed = true;
    }

    if (!d.containsKey("onCode")) {
      d["onCode"] = "";
      changed = true;
    }
    if (!d.containsKey("offCode")) {
      d["offCode"] = "";
      changed = true;
    }
    if (!d.containsKey("threadOnCode")) {
      d["threadOnCode"] = "";
      changed = true;
    }

    if (!d.containsKey("onBits")) {
      d["onBits"] = 0;
      changed = true;
    }
    if (!d.containsKey("onProto")) {
      d["onProto"] = 0;
      changed = true;
    }
    if (!d.containsKey("offBits")) {
      d["offBits"] = 0;
      changed = true;
    }
    if (!d.containsKey("offProto")) {
      d["offProto"] = 0;
      changed = true;
    }
    if (!d.containsKey("threadOnBits")) {
      d["threadOnBits"] = 0;
      changed = true;
    }
    if (!d.containsKey("threadOnProto")) {
      d["threadOnProto"] = 0;
      changed = true;
    }
  }

  if (changed) {
    return saveConfigAtomic();
  }

  return true;
}

static void relayOn() {
  // digitalWrite(RELAY1_SIREN_PIN, HIGH);
  alarmUntil = millis() + ALARM_MS;
}

static void relayOff() {
  // digitalWrite(RELAY1_SIREN_PIN, LOW);
  alarmUntil = 0;
}

void updateSOSLightStatus() {
  JsonArray rooms = configDoc["sos_devices"].as<JsonArray>();
  bool anyOn = false;

  for (JsonObject room : rooms) {
    const char* status = room["status"] | "OFF";
    if (strcmp(status, "ON") == 0) {
      anyOn = true;
      break;
    }
  }

  // digitalWrite(RELAY2_ALARM_LIGHT_PIN, anyOn ? HIGH : LOW);
}

static void publishSosEvent(JsonObject d, const char* newStatus, const char* sourceType = "on") {
  DynamicJsonDocument payload(4000);
  payload["type"] = "sos";
  payload["serialNumber"] = String((const char*)(configDoc["device_serial_number"] | ""));
  payload["id"] = d["id"] | 0;
  payload["name"] = String((const char*)(d["name"] | ""));
  payload["roomId"] = d["roomId"] | 0;
  payload["status"] = newStatus;   // ON or OFF only
  payload["source"] = sourceType;  // on / thread_on / off
  payload["timestampMs"] = (uint32_t)millis();

  String codeToSend = "";

  if (strcmp(sourceType, "thread_on") == 0) {
    codeToSend = String((const char*)(d["threadOnCode"] | ""));
  } else if (strcmp(newStatus, "ON") == 0) {
    codeToSend = String((const char*)(d["onCode"] | ""));
  } else {
    codeToSend = String((const char*)(d["offCode"] | ""));
  }

  payload["code"] = codeToSend;

  String out;
  serializeJson(payload, out);

  mqttSOSAlarmNotification(out);

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
    String threadOnC = String((const char*)(d["threadOnCode"] | ""));

    // Normal SOS ON
    if (onC.length() && codeStr == onC) {
      relayOn();
      d["status"] = "ON";
      d["lastSeen"] = (uint32_t)millis();
      changed = true;
      publishSosEvent(d, "ON", "on");
      break;
    }

    // SOS OFF
    if (offC.length() && codeStr == offC) {
      relayOff();
      d["status"] = "OFF";
      d["lastSeen"] = (uint32_t)millis();
      changed = true;
      publishSosEvent(d, "OFF", "off");
      break;
    }

    // Threaded SOS ON, functionally also ON
    if (threadOnC.length() && codeStr == threadOnC) {
      relayOn();
      d["status"] = "ON";
      d["lastSeen"] = (uint32_t)millis();
      changed = true;
      publishSosEvent(d, "ON", "thread_on");
      break;
    }
  }

  if (changed) {
    saveConfigAtomic();
  }
}

// ---------- Routes ----------
static void setupRoutes() {
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
      loadingConfigFile = false;
      return;
    }

    String header = readFile("/header.html");
    String html = readFile(UI_SOSHTML_PATH);
    String full = header + html;
    full = replaceHeaderContent(full);

    server.send(200, "text/html", full);
    delay(200);
    loadingConfigFile = false;
  });

  server.on("/DeviceSOSRooms.html", HTTP_GET, []() {
    if (!LittleFS.exists(UI_SOSHTML_PATH)) {
      server.send(404, "text/html", "DeviceSOSRooms.html not found");
      return;
    }
    File f = LittleFS.open(UI_SOSHTML_PATH, "r");
    if (!f) {
      server.send(500, "text/html", "Failed to open DeviceSOSRooms.html");
      return;
    }
    server.streamFile(f, "text/html");
    f.close();
  });

  server.on("/DeviceSOSRooms.js", HTTP_GET, []() {
    if (!LittleFS.exists(UI_SOSJS_PATH)) {
      server.send(404, "application/javascript", "// DeviceSOSRooms.js not found");
      return;
    }

    File f = LittleFS.open(UI_SOSJS_PATH, "r");
    if (!f) {
      server.send(500, "application/javascript", "// failed to open DeviceSOSRooms.js");
      return;
    }

    server.streamFile(f, "application/javascript");
    f.close();
  });

  server.on("/api/sos/devices", HTTP_GET, []() {
    loadConfig();
    DynamicJsonDocument out(8192);
    out["devices"] = configDoc["sos_devices"];
    sendJson(200, out);
  });

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

  server.on("/api/sos/devices/upsert", HTTP_POST, []() {
    DynamicJsonDocument in(4096);
    if (!parseBodyJson(in)) {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    loadConfig();

    int id = in["id"] | 0;
    String name = String((const char*)(in["name"] | ""));
    int roomId = in["roomId"] | 0;

    String onCode = String((const char*)(in["onCode"] | ""));
    String offCode = String((const char*)(in["offCode"] | ""));
    String threadOnCode = String((const char*)(in["threadOnCode"] | ""));

    int onBits = in["onBits"] | 0;
    int onProto = in["onProto"] | 0;
    int offBits = in["offBits"] | 0;
    int offProto = in["offProto"] | 0;
    int threadOnBits = in["threadOnBits"] | 0;
    int threadOnProto = in["threadOnProto"] | 0;

    JsonObject d;
    if (id > 0) {
      d = findDeviceById(id);
      if (d.isNull()) {
        id = 0;
      }
    }

    if (id == 0) {
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

    if (onCode.length() && codeExistsInOtherDevice(onCode, id)) {
      server.send(409, "application/json", "{\"error\":\"onCode already exists\"}");
      return;
    }
    if (offCode.length() && codeExistsInOtherDevice(offCode, id)) {
      server.send(409, "application/json", "{\"error\":\"offCode already exists\"}");
      return;
    }
    if (threadOnCode.length() && codeExistsInOtherDevice(threadOnCode, id)) {
      server.send(409, "application/json", "{\"error\":\"threadOnCode already exists\"}");
      return;
    }

    if (onCode.length() && offCode.length() && onCode == offCode) {
      server.send(400, "application/json", "{\"error\":\"onCode and offCode cannot be same\"}");
      return;
    }
    if (onCode.length() && threadOnCode.length() && onCode == threadOnCode) {
      server.send(400, "application/json", "{\"error\":\"onCode and threadOnCode cannot be same\"}");
      return;
    }
    if (offCode.length() && threadOnCode.length() && offCode == threadOnCode) {
      server.send(400, "application/json", "{\"error\":\"offCode and threadOnCode cannot be same\"}");
      return;
    }

    if (name.length()) d["name"] = name;
    d["roomId"] = roomId;

    if (onCode.length()) d["onCode"] = onCode;
    if (offCode.length()) d["offCode"] = offCode;
    if (threadOnCode.length()) d["threadOnCode"] = threadOnCode;

    if (in.containsKey("onBits")) d["onBits"] = onBits;
    if (in.containsKey("onProto")) d["onProto"] = onProto;
    if (in.containsKey("offBits")) d["offBits"] = offBits;
    if (in.containsKey("offProto")) d["offProto"] = offProto;
    if (in.containsKey("threadOnBits")) d["threadOnBits"] = threadOnBits;
    if (in.containsKey("threadOnProto")) d["threadOnProto"] = threadOnProto;

    if (!d.containsKey("onCode")) d["onCode"] = "";
    if (!d.containsKey("offCode")) d["offCode"] = "";
    if (!d.containsKey("threadOnCode")) d["threadOnCode"] = "";

    if (!d.containsKey("onBits")) d["onBits"] = 0;
    if (!d.containsKey("onProto")) d["onProto"] = 0;
    if (!d.containsKey("offBits")) d["offBits"] = 0;
    if (!d.containsKey("offProto")) d["offProto"] = 0;
    if (!d.containsKey("threadOnBits")) d["threadOnBits"] = 0;
    if (!d.containsKey("threadOnProto")) d["threadOnProto"] = 0;

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
  delay(200);

  loadConfig();

  if (!LittleFS.exists(CONFIG_PATH)) {
    saveConfigAtomic();
  }

  forceAllSosOffAndSave(false);

  delay(200);

  rf.enableReceive(digitalPinToInterrupt(RF_RX_PIN));
  Serial.println("RF receiver enabled - 3 button SOS mode (ON / OFF / THREAD ON->ON)");

  setupRoutes();
  server.begin();
  Serial.println("HTTP server started on port 80");
}

void loopSosDevice() {
  server.handleClient();

  if (alarmUntil && millis() > alarmUntil) {
    relayOff();
    Serial.println("Relay auto OFF (timeout)");
  }

  if (rf.available()) {
    uint32_t code = rf.getReceivedValue();
    uint16_t bits = rf.getReceivedBitlength();
    uint8_t proto = rf.getReceivedProtocol();
    uint16_t pulse = rf.getReceivedDelay();
    rf.resetAvailable();

    if (code == 0) return;

    uint32_t now = millis();

    if (code == lastCode && (now - lastAt) < DUP_IGNORE_MS) return;
    lastCode = code;
    lastAt = now;

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

    applyRfToDevicesAndRelay(code, bits, proto, pulse);
    firstLoad = false;
  }
}