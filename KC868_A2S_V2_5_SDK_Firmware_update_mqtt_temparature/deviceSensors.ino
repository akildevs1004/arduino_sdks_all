#include <Wire.h>

// #define MAX485_DE 25  // RS485 DE/RE control pin
// #define RS485_TX 13   // RS485 TX pin
// #define RS485_RX 16   // RS485 RX pin

#define MAX485_DE 25  // RS485 DE/RE control pin
#define RS485_TX 32   // RS485 TX pin
#define RS485_RX 35   // RS485 RX pin

ModbusMaster sensor1;  // Address 4 (changed)
ModbusMaster sensor2;  // Address 2
ModbusMaster sensor3;  // Address 3

// -------------------- I2C --------------------
#define I2C_SDA 4
#define I2C_SCL 16

// Maximum number of sensors
#define MAX_SENSORS 2;

struct SensorEntry {
  uint8_t id;
  ModbusMaster modbus;
  float temperature = 0.0;
  float humidity = 0.0;
  bool isOnline = false;
  float previousTemperature;
  bool previousAlarm;
};
SensorEntry sensors[10];


int sensorCount = 0;



unsigned long lastSensorReadTime = 0;
long temperature_read_interval = 60;  // 10 seconds

void preTransmission() {
  // digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  // digitalWrite(MAX485_DE, 0);
}



void DeviceSetup() {
  // Serial.begin(115200);
  // Serial2.begin(4800, SERIAL_8N1, RS485_RX, RS485_TX);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);


  // pinMode(MAX485_DE, OUTPUT);
  // digitalWrite(MAX485_DE, 0);

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  sensorCount = config["max_temperature_sensor_count"];

  JsonArray alerts = config["temperature_alerts_config"].as<JsonArray>();


  Serial.println("Sensor Count max_temperature_sensor_count " + String(sensorCount));

  for (int i = 0; i < sensorCount; i++) {
    Serial.print("sensor_address_id = ");
    Serial.println(alerts[i]["sensor_address_id"] | -1);
    sensors[i].id = alerts[i]["sensor_address_id"] | -1;
    sensors[i].modbus.begin(alerts[i]["sensor_address_id"], Serial2);
    sensors[i].modbus.preTransmission(preTransmission);
    sensors[i].modbus.postTransmission(postTransmission);
    Serial.printf("🔧 Sensor initialized (ID: %d)\n", sensors[i].id);
  }



  delay(1000);

  relaysSetup();

  // testReadTemperatureOnce();
}

// Add temperature alarm if threshold exceeded
unsigned long lastTempAlarmTime = 0;                 // Define this globally or statically
const unsigned long TEMP_ALARM_INTERVAL = 1000 * 1;  // 1 minute in milliseconds
float temperature, humidity;
void readAllSensors() {

  if (!readConfig("config.json")) {
    Serial.println("readConfig failed");
    return;
  }

  float diffInTemperature = config["temperature_difference"] | 0.2f;

  JsonArray alerts = config["temperature_alerts_config"].as<JsonArray>();
  if (alerts.isNull()) {
    Serial.println("temperature_alerts_config Not Found");
    return;
  }

  int sensorCountCfg = config["max_temperature_sensor_count"] | (int)alerts.size();
  int count = min(sensorCountCfg, (int)alerts.size());  // ✅ safe

  Serial.println("readAllSensors - count=" + String(count));

  for (int i = 0; i < count; i++) {

    JsonObject rule = alerts[i];
    int sid = rule["sensor_address_id"] | -1;


    if (sid <= 0) {
      Serial.println("readAllSensors - sid is ZERO");

      continue;
    };




    // Read Modbus
    uint8_t r = sensors[i].modbus.readInputRegisters(0x0000, 2);

    if (r != sensors[i].modbus.ku8MBSuccess) {
      sensors[i].isOnline = false;
      Serial.printf("❌ Sensor %d OFFLINE (err=%d)\n", sid, r);
      delay(200);
      continue;
    }

    uint16_t tempRaw = sensors[i].modbus.getResponseBuffer(1);
    uint16_t humRaw = sensors[i].modbus.getResponseBuffer(0);

    float temperature = (tempRaw < 10000) ? tempRaw * 0.1f : -1.0f * (tempRaw - 10000) * 0.1f;
    float humidity = humRaw * 0.1f;

    Serial.print("🔥 Temperature: ");
    Serial.println(temperature);

    bool isAlarmActiveThisSensor = false;

    // Build payload (bigger doc!)
    StaticJsonDocument<256> doc;
    String jsonTempData;

    doc["serialNumber"] = device_serial_number;
    doc["sensor_serial_address"] = sid;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["timestamp"] = millis();
    doc["type"] = "sensor";
    doc["temperature_alarm"] = 0;
    doc["humidity_alarm"] = 0;

    // ---- Use rule directly (no findSensorById) ----
    bool tempEnabled = rule["temperature"]["enabled"] | false;
    if (tempEnabled) {
      float minTemp = rule["temperature"]["min"].isNull() ? -999.0f : rule["temperature"]["min"].as<float>();
      float maxTemp = rule["temperature"]["max"].isNull() ? 999.0f : rule["temperature"]["max"].as<float>();



      if (temperature <= minTemp || temperature >= maxTemp) {
        unsigned long now = millis();
        if (lastTempAlarmTime == 0 || (now - lastTempAlarmTime) >= TEMP_ALARM_INTERVAL) {
          doc["type"] = "alarm";
          doc["temperature_alarm"] = 1;
          doc["temperature_min"] = minTemp;
          doc["temperature_max"] = maxTemp;
          lastTempAlarmTime = now;

          callRelayBuzzerTurn(true);
          isAlarmActiveThisSensor = true;
          Serial.println("🔥 Temperature alarm sent");
        } else {
          Serial.println("⏱ Temp alarm suppressed");
        }
      }
    }

    bool humEnabled = rule["humidity"]["enabled"] | false;
    if (humEnabled) {
      float minHum = rule["humidity"]["min"].isNull() ? -999.0f : rule["humidity"]["min"].as<float>();
      float maxHum = rule["humidity"]["max"].isNull() ? 999.0f : rule["humidity"]["max"].as<float>();

      if (humidity <= minHum || humidity >= maxHum) {
        unsigned long now = millis();
        if (lastTempAlarmTime == 0 || (now - lastTempAlarmTime) >= TEMP_ALARM_INTERVAL) {
          doc["type"] = "alarm";
          doc["humidity_alarm"] = 1;
          doc["humidity_min"] = minHum;
          doc["humidity_max"] = maxHum;
          lastTempAlarmTime = now;

          callRelayBuzzerTurn(true);
          isAlarmActiveThisSensor = true;
          Serial.println("🔥 Humidity alarm sent");
        } else {
          Serial.println("⏱ Hum alarm suppressed");
        }
      }
    }

    // Buzzer OFF only if no alarm + no DI alarms
    if (!isAlarmActiveThisSensor && !checkAnyAlarmOpen()) {
      callRelayBuzzerTurn(false);
    }

    serializeJson(doc, jsonTempData);

    // Send only on change / alarm change
    if (fabs(temperature - sensors[i].temperature) >= diffInTemperature || fabs(temperature - sensors[i].previousTemperature) >= diffInTemperature || sensors[i].previousAlarm != isAlarmActiveThisSensor) {

      sendTemperatureDataToServerHttp(jsonTempData);
    }

    // Update cache
    sensors[i].previousTemperature = sensors[i].temperature;
    sensors[i].temperature = temperature;
    sensors[i].humidity = humidity;
    sensors[i].isOnline = true;
    sensors[i].previousAlarm = isAlarmActiveThisSensor;

    delay(200);
  }
}
/*
void testReadTemperatureOnce() {

  static const uint16_t REG_START = 0x0000;
static const uint16_t REG_COUNT = 2;
  Serial.println();
  Serial.println("=== SIMPLE TEMPERATURE TEST (REFERENCE STYLE) ===");

  // RS485 UART (same as reference)
  Serial2.end();
  delay(200);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);

  // Use the same empty callbacks (auto-direction RS485)
  sensor1.preTransmission(preTransmission);   // preTransmission() is EMPTY
  sensor1.postTransmission(postTransmission); // postTransmission() is EMPTY

  delay(500);

  const uint8_t slaveId = 4;
  bool usedHolding = false;

  Serial.print("Testing Slave ID: ");
  Serial.println(slaveId);

  // Try Input then Holding (same logic)
  sensor1.begin(slaveId, Serial2);

  uint8_t res = sensor1.readInputRegisters(REG_START, REG_COUNT);
  if (res == sensor1.ku8MBSuccess) {
    usedHolding = false;
  } else {
    res = sensor1.readHoldingRegisters(REG_START, REG_COUNT);
    if (res == sensor1.ku8MBSuccess) usedHolding = true;
  }

  if (res != sensor1.ku8MBSuccess) {
    Serial.print("❌ Modbus read failed. Error code: ");
    Serial.println(res);
    Serial.println("TIP: check baud=9600, slaveId=4, A/B wiring, common GND.");
    Serial.println("===============================================");
    return;
  }

  Serial.print("✅ Read OK using: ");
  Serial.println(usedHolding ? "HOLDING (0x03)" : "INPUT (0x04)");

  // IMPORTANT: your reference uses buffer(0)=temp, buffer(1)=hum
  uint16_t temperatureRaw = sensor1.getResponseBuffer(0);
  uint16_t humidityRaw    = sensor1.getResponseBuffer(1);

  float temperature = decodeTemperature(temperatureRaw);
  float humidity    = decodeHumidity(humidityRaw);

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %RH");

  Serial.println("===============================================");
}

*/
// Decode logic (same as your original)
float decodeTemperature(uint16_t raw) {
  if (raw < 10000) return raw * 0.1f;
  return -1.0f * (raw - 10000) * 0.1f;
}
float decodeHumidity(uint16_t raw) {
  return raw * 0.1f;
}
// JsonObject findSensorById(JsonArray array, int targetId) {
//   for (JsonObject item : array) {
//     if (item["sensor_address_id"] == targetId) {
//       return item;
//     }
//   }
//   return JsonObject();  // Empty
// }
// Build JSON output with all sensor values
String buildSensorJson() {
  StaticJsonDocument<126> doc;
  doc["serial_number"] = device_serial_number;
  JsonArray arr = doc.createNestedArray("sensors");

  for (int i = 0; i < sensorCount; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["id"] = sensors[i].id;
    obj["temperature"] = sensors[i].temperature;
    obj["humidity"] = sensors[i].humidity;
    obj["online"] = sensors[i].isOnline;
  }

  String output;
  serializeJson(doc, output);
  return output;
}
void Deviceloop() {
  unsigned long currentMillis = millis();
  // digitalLoop();
  relayLoop();
  if (config["temperature_read_interval"])
    temperature_read_interval = config["temperature_read_interval"];  //  | 60;  // default to 60 if missing

  // Serial.print(currentMillis - lastSensorReadTime);

  // Serial.print(" - ");
  // Serial.println(1000 * temperature_read_interval);


  if (currentMillis - lastSensorReadTime >= (1000 * temperature_read_interval)) {

    // Serial.print(" READING TEMPERATURE SENSORS -------------------------------------------------------------------- ");
    lastSensorReadTime = currentMillis;

    // Read all sensors
    if (!loadingConfigFile)
      readAllSensors();
  }



  ///networkLoop();
}
