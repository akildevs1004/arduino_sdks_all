

#define MAX485_DE 25  // RS485 DE/RE control pin
#define RS485_TX 13   // RS485 TX pin
#define RS485_RX 16   // RS485 RX pin

ModbusMaster sensor1;  // Address 4 (changed)
ModbusMaster sensor2;  // Address 2
ModbusMaster sensor3;  // Address 3

// Maximum number of sensors
#define MAX_SENSORS 2;

struct SensorEntry {
  uint8_t id;
  ModbusMaster modbus;
  float temperature = 0.0;
  float humidity = 0.0;
  bool isOnline = false;
  float previousTemperature;
};
SensorEntry sensors[10];


int sensorCount = 0;



unsigned long lastSensorReadTime = 0;
long temperature_read_interval = 60;  // 10 seconds

void preTransmission() {
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_DE, 0);
}

void DeviceSetup() {
  // Serial.begin(115200);
  Serial2.begin(4800, SERIAL_8N1, RS485_RX, RS485_TX);

  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_DE, 0);



  sensorCount = config["max_temperature_sensor_count"];


  Serial.println("Sensor Count max_temperature_sensor_count " + String(sensorCount));

  for (int i = 0; i < sensorCount; i++) {
    sensors[i].id = i + 1;
    sensors[i].modbus.begin(i + 1, Serial2);
    sensors[i].modbus.preTransmission(preTransmission);
    sensors[i].modbus.postTransmission(postTransmission);
    Serial.printf("🔧 Sensor initialized (ID: %d)\n", sensors[i].id);
  }
  // // Initialize sensors with respective addresses
  // sensor1.begin(1, Serial2);  // ✅ Changed to address 4
  // sensor1.preTransmission(preTransmission);
  // sensor1.postTransmission(postTransmission);

  // sensor2.begin(2, Serial2);
  // sensor2.preTransmission(preTransmission);
  // sensor2.postTransmission(postTransmission);

  // sensor3.begin(3, Serial2);
  // sensor3.preTransmission(preTransmission);
  // sensor3.postTransmission(postTransmission);


  delay(1000);

  relaysSetup();
  digitalSetup();
}
// Add temperature alarm if threshold exceeded
unsigned long lastTempAlarmTime = 0;                 // Define this globally or statically
const unsigned long TEMP_ALARM_INTERVAL = 1000 * 1;  // 1 minute in milliseconds
float temperature, humidity;
void readAllSensors() {



  String jsonTempData = "";
  float diffInTemperature = 0.2;
  if (config.containsKey("temperature_difference")) {
    diffInTemperature = config["temperature_difference"].as<float>();
  }
  uint16_t tempRaw, humRaw;

  bool temperatureChanged = false;
  sensorCount = config["max_temperature_sensor_count"];
  Serial.println("readAllSensors - " + String(sensorCount));

  for (int i = 0; i < sensorCount; i++) {
    {
      uint8_t result = sensors[i].modbus.readInputRegisters(0x0000, 2);

      if (result == sensors[i].modbus.ku8MBSuccess) {
        tempRaw = sensors[i].modbus.getResponseBuffer(1);
        humRaw = sensors[i].modbus.getResponseBuffer(0);

        temperature = (tempRaw < 10000) ? tempRaw * 0.1 : -1 * (tempRaw - 10000) * 0.1;
        humidity = humRaw * 0.1;

        Serial.println("readAllSensors-Data " + String(i + 1) + " " + String(temperature) + " " + String(humidity));


        // 🔍 Compare with previous temperature
        if (abs(temperature - sensors[i].temperature) >= diffInTemperature && temperature != sensors[i].temperature) {
          Serial.println("readAllSensors Changed " + String(i) + " " + String(sensors[i].temperature) + " " + String(temperature) + " - " + String(diffInTemperature));


          temperatureChanged = true;


          //trigger Server API
          // Base payload
          StaticJsonDocument<64> doc;
          doc["serialNumber"] = device_serial_number;
          doc["humidity"] = humidity;
          doc["temperature"] = temperature;
          doc["sensor_serial_address"] = i + 1;
          doc["type"] = "sensor";
          doc["temperature_alarm"] = 0;



          Serial.print(String(config["temp_checkbox"]));
          Serial.print("-----------");
          Serial.print(String(config["min_temperature"]));
          Serial.print("-----------");
          Serial.println(temperature);  // Fix: typo in `prinln`


          //get settings of Temperature sensor address 1 details
          // DynamicJsonDocument doc(2048);
          // deserializeJson(doc, json);
          if (config["temperature_alerts_config"]) {
            JsonObject result = findSensorById(config["temperature_alerts_config"], i + 1);

            if (!result.isNull()) {
              Serial.println("Sensor Address Settings found:");
              serializeJsonPretty(result, Serial);

              if (result["temperature"]["enabled"]) {
                float minTemp = result["temperature"]["min"];
                float maxTemp = result["temperature"]["max"];

                Serial.print(i + 1);

                Serial.print(" - Min Temp: ");
                Serial.println(minTemp);
                Serial.print("Max Temp: ");
                Serial.println(maxTemp);

                if (temperature <= minTemp || temperature >= maxTemp) {
                  unsigned long currentTime = millis();
                  if (currentTime - lastTempAlarmTime >= TEMP_ALARM_INTERVAL || lastTempAlarmTime == 0) {
                    doc["temperature_alarm"] = 1;
                    doc["type"] = "alarm";
                    doc["temperature_max"] = maxTemp;
                    doc["temperature_min"] = minTemp;
                    lastTempAlarmTime = currentTime;
                    Serial.println("🔥 Temperature alarm sent--------------------------------------------");
                  } else {
                    Serial.println("⏱ Alarm suppressed (waiting interval)");
                  }
                }
              }
              if (result["humidity"]["enabled"]) {
                float minTemp = result["humidity"]["min"];
                float maxTemp = result["humidity"]["max"];

                Serial.print("Min Temp: ");
                Serial.println(minTemp);
                Serial.print("Max Temp: ");
                Serial.println(maxTemp);

                if (humidity <= minTemp || humidity >= maxTemp) {
                  unsigned long currentTime = millis();
                  if (currentTime - lastTempAlarmTime >= TEMP_ALARM_INTERVAL || lastTempAlarmTime == 0) {
                    doc["humidity_alarm"] = 1;
                    doc["type"] = "alarm";
                    doc["humidity_max"] = maxTemp;
                    doc["humidity_min"] = minTemp;
                    lastTempAlarmTime = currentTime;
                    Serial.println("🔥 Humidity alarm sent--------------------------------------------");
                  } else {
                    Serial.println("⏱ Humidity suppressed (waiting interval)");
                  }
                }
              }
            } else {
              Serial.println("Sensor not found.");
            }
          } else {
            Serial.println("temperature_alerts_config Not Found");
          }



          serializeJson(doc, jsonTempData);


          //Serial.println("Sending: " + jsonTempData);


          sendAlarmTriggerToSocketserver(jsonTempData);
          if (config["http_communication"])
            sendTemperatureDataToServerHttp(jsonTempData);
        }

        sensors[i].previousTemperature = sensors[i].temperature;
        sensors[i].temperature = temperature;
        sensors[i].humidity = humidity;
        sensors[i].isOnline = true;


      } else {
        sensors[i].isOnline = false;



        Serial.println("❌ Sensor  readAllSensors-Data " + String(i + 1) + " OFFLINE ");
      }


    }  //checking config file loading

    delay(1000);
  }  //for loop


  if (temperatureChanged) {
    sensorData = buildSensorJson();
  }
}

JsonObject findSensorById(JsonArray array, int targetId) {
  for (JsonObject item : array) {
    if (item["sensor_address_id"] == targetId) {
      return item;
    }
  }
  return JsonObject();  // Empty
}
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
  digitalLoop();
  relayLoop();
  if (config["temperature_read_interval"])
    temperature_read_interval = config["temperature_read_interval"];  //  | 60;  // default to 60 if missing

  // Serial.print(currentMillis - lastSensorReadTime);

  // Serial.print(" - ");
  // Serial.println(1000 * temperature_read_interval);


  if (currentMillis - lastSensorReadTime >= (1000 * temperature_read_interval)) {

    Serial.print(" READING TEMPERATURE SENSORS -------------------------------------------------------------------- ");
    lastSensorReadTime = currentMillis;

    // Read all sensors
    //if (!loadingConfigFile)
    readAllSensors();
  }



  ///networkLoop();
}
