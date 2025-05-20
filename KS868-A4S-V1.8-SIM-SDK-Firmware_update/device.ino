

#define MAX485_DE 25  // RS485 DE/RE control pin
#define RS485_TX 13   // RS485 TX pin
#define RS485_RX 15   // RS485 RX pin

ModbusMaster sensor1;  // Address 4 (changed)
ModbusMaster sensor2;  // Address 2
ModbusMaster sensor3;  // Address 3

// Maximum number of sensors
#define MAX_SENSORS 10;

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
const unsigned long sensorReadInterval = 1000 * 5;  // 10 seconds

void preTransmission() {
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_DE, 0);
}

void DeviceSetup() {
  // Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);

  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_DE, 0);



  sensorCount = config["max_temperature_sensor_count"];


  Serial.println("Sensor Count max_temperature_sensor_count" + String(sensorCount));

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
unsigned long lastTempAlarmTime = 0;              // Define this globally or statically
const unsigned long TEMP_ALARM_INTERVAL = 60000;  // 1 minute in milliseconds

void readAllSensors() {

  if (!loadingConfigFile) {

    String jsonTempData;
    float diffInTemperature = 0.2;
    if (config.containsKey("temperature_difference")) {
      diffInTemperature = config["temperature_difference"].as<float>();
    }
    uint16_t tempRaw, humRaw;
    float temperature, humidity;
    bool temperatureChanged = false;
    sensorCount = config["max_temperature_sensor_count"];
    Serial.println("readAllSensors" + String(sensorCount));
    for (int i = 0; i < sensorCount; i++) {
      uint8_t result = sensors[i].modbus.readInputRegisters(0x0000, 2);

      if (result == sensors[i].modbus.ku8MBSuccess) {
        tempRaw = sensors[i].modbus.getResponseBuffer(0);
        humRaw = sensors[i].modbus.getResponseBuffer(1);

        temperature = (tempRaw < 10000) ? tempRaw * 0.1 : -1 * (tempRaw - 10000) * 0.1;
        humidity = humRaw * 0.1;

        Serial.println("readAllSensors-Data " + String(i) + " " + String(temperature)+ " " + String(humidity));


       // Serial.println("readAllSensors-compariosn " + String(i) + " " + String(diffInTemperature));

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
          doc["sensor_serial_number"] = i;
          doc["type"] = "sensor";
          doc["temperature_alarm"] = 0;



          Serial.print(String(config["temp_checkbox"]));
          Serial.print("-----------");
          Serial.print(String(config["min_temperature"]));
          Serial.print("-----------");
          Serial.println(temperature);  // Fix: typo in `prinln`


          // Inside your function or loop
          if (config["temp_checkbox"]) {
            float minTemp = config["min_temperature"];
            float maxTemp = config["max_temperature"];




            if (temperature <= minTemp || temperature >= maxTemp) {


              unsigned long currentTime = millis();
              Serial.println(currentTime);
              Serial.println(lastTempAlarmTime);
              Serial.println(TEMP_ALARM_INTERVAL);
              if (currentTime - lastTempAlarmTime >= TEMP_ALARM_INTERVAL || lastTempAlarmTime == 0) {
                doc["temperature_alarm"] = 1;
                doc["type"] = "alarm";
                lastTempAlarmTime = currentTime;
                Serial.println("🔥 Temperature alarm sent--------------------------------------------");


                delay(1000);
              } else {
                Serial.println("⏱ Alarm suppressed (waiting 1 minute)");
              }
            }
          }


          serializeJson(doc, jsonTempData);

          //         char jsonTempData[64];
          // snprintf(jsonTempData, sizeof(jsonTempData),
          //          "{\"serialNumber\":\"%s\",\"humidity\":%.1f,\"temperature\":%.1f,\"sensor_serial_number\":%d%s}",
          //          device_serial_number, humidity, temperature, i,
          //          (temperature <= config["min_temperature"] || temperature >= config["max_temperature"]) ? ",\"temperature_alarm\":\"1\"" : "");

          Serial.println("Sending: " + jsonTempData);

          //// sendTemperatureDataToServer(jsonTempData);
          // // sendPostRequest("/api/alarm_device_status",jsonTempData);
          if (config["http_trigger_alarm"])
            sendTemperatureDataToServerHttp(jsonTempData);
          sendAlarmTriggerToSocketserver(jsonTempData);
        }

        sensors[i].previousTemperature = sensors[i].temperature;
        sensors[i].temperature = temperature;
        sensors[i].humidity = humidity;
        sensors[i].isOnline = true;


      } else {
        sensors[i].isOnline = false;
      }

      


    }  //for loop


    if (temperatureChanged) {
      sensorData = buildSensorJson();
    }
  }
}
/*
void readAllSensors() {
  uint16_t tempRaw, humRaw;
  float temperature, humidity;
  for (int i = 0; i < sensorCount; i++) {
    uint8_t result = sensors[i].modbus.readInputRegisters(0x0000, 2);  // Adjust based on your sensor's register map

    if (result == sensors[i].modbus.ku8MBSuccess) {

      tempRaw = sensors[i].modbus.getResponseBuffer(0);
      humRaw = sensors[i].modbus.getResponseBuffer(1);

      if (tempRaw < 10000)
        temperature = tempRaw * 0.1;
      else
        temperature = -1 * (tempRaw - 10000) * 0.1;

      humidity = humRaw * 0.1;

      sensors[i].temperature = temperature;
      sensors[i].humidity = humidity;
      sensors[i].isOnline = true;

      // Serial.printf("📡 Sensor ID %d - Temp: %.2f °C, Hum: %.2f %%\n",
      //               sensors[i].id, sensors[i].temperature, sensors[i].humidity);
    } else {
      sensors[i].isOnline = false;
      // Serial.printf("⚠️ Sensor ID %d - Read failed (code: %d)\n",
      //               sensors[i].id, result);
    }
    sensorData = buildSensorJson();
    // Serial.print("📦 Sensor JSON:");
    // Serial.println(sensorData);




    delay(100);
  }
}
*/
// String buildSensorJson() {
//   StaticJsonDocument<768> doc;  // Adjust size as needed

//   doc["serial"] = device_serial_number;

//   JsonArray sensorArray = doc.createNestedArray("sensors");

//   for (int i = 0; i < sensorCount; i++) {
//     JsonObject sensorObj = sensorArray.createNestedObject();
//     sensorObj["id"] = sensors[i].id;
//     sensorObj["temperature"] = sensors[i].temperature;
//     sensorObj["humidity"] = sensors[i].humidity;
//     sensorObj["online"] = sensors[i].isOnline;
//   }

//   String output;
//   serializeJson(doc, output);
//   return output;
// }
// void readSensor(ModbusMaster& node, const char* name) {
//   uint8_t result;
//   uint16_t tempRaw, humRaw;
//   float temperature, humidity;

//   result = node.readHoldingRegisters(0x0000, 2);  // Temp @ 0x0000, Hum @ 0x0001

//   if (result == node.ku8MBSuccess) {
//     tempRaw = node.getResponseBuffer(0);
//     humRaw = node.getResponseBuffer(1);

//     if (tempRaw < 10000)
//       temperature = tempRaw * 0.1;
//     else
//       temperature = -1 * (tempRaw - 10000) * 0.1;

//     humidity = humRaw * 0.1;

//     Serial.print(name);
//     Serial.print(" - Temp: ");
//     Serial.print(temperature);
//     Serial.print(" °C, Hum: ");
//     Serial.print(humidity);
//     Serial.println(" %");

//     //update to Sensor data

//     StaticJsonDocument<512> doc;
//     doc["serial"] = deviceSerial;

//     JsonArray sensorsArray = doc.createNestedArray("sensors");

//     for (int i = 0; i < 4; i++) {
//       JsonObject s = sensorsArray.createNestedObject();
//       s["id"] = sensors[i].name;
//       s["name"] = sensors[i].name;

//       s["temperature"] = sensors[i].temperature;
//       s["humidity"] = sensors[i].humidity;
//     }

//     String output;
//     serializeJson(doc, output);
//     Serial.println("JSON Output:");
//     Serial.println(output);  // You can send this via HTTP




//   } else {
//     Serial.print(name);
//     Serial.print(" - Read Error: ");
//     Serial.println(result, HEX);
//   }
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

  if (currentMillis - lastSensorReadTime >= sensorReadInterval) {
    lastSensorReadTime = currentMillis;

    // Read all sensors
    readAllSensors();
    // Print full JSON to Serial

    // readSensor(sensor1, "address1", 1);
    // delay(100);
    // readSensor(sensor2, "address2", 2);
    // delay(100);
    // readSensor(sensor3, "address3", 3);
    // delay(100);
  }

  relayLoop();
  digitalLoop();

  ///networkLoop();
}
