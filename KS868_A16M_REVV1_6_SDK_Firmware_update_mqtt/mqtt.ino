

// #define MQTT_MAX_PACKET_SIZE 2048  // or 1024, or more
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi credentials
// const char* ssid = "akil";          // Enter your WiFi name
// const char* password = "Akil1234";  // Enter WiFi password

// MQTT Broker
const char* mqtt_server = "broker.hivemq.com";
int mqtt_port = 1883;
String clientId = "xtremevision";

// Device unique ID (serial number)
// const char* device_serial = "XT123456";

// MQTT topics (based on serial)
String MqttTopic_sub = clientId + "/" + device_serial_number + "/config/request";
String MqttTopic_pub = clientId + "/" + device_serial_number + "/config";
String MqttTopic_pubheartbeat = clientId + "/" + device_serial_number + "/heartbeat";
;
// std::string topic_heartbeat_str = std::string("device/") + std::string(device_serial_number.c_str()) + "/heartbeat";

// const char* topic_heartbeat = topic_heartbeat_str.c_str();

WiFiClient espClient;
PubSubClient mqttclient(espClient);
StaticJsonDocument<256> mqttconfig;

// std::string topic_heartbeat_str = std::string("device/") + String(device_serial_number) + "/heartbeat";
// const char* topic_heartbeat = topic_heartbeat_str.c_str();




unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 10000;  // 10 seconds




//request from cloud server
void MqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String msg = (char*)payload;
  Serial.printf("Received [%s]: %s\n", topic, msg.c_str());

  if (msg == "GET_CONFIG") {

    DynamicJsonDocument mqttconfig(1024);
    mqttconfig["serialNumber"] = device_serial_number;
    mqttconfig["type"] = "config";
    mqttconfig["config"] = readConfig("config.json");  //deviceConfigContent;  //;
    // // mqttconfig["sensor_data"] = sensorData;      //


    String payload;
    // payload=readFile("/test.json");
    serializeJson(mqttconfig, payload);
    bool sent = mqttclient.publish(MqttTopic_pub.c_str(), payload.c_str());
    if (sent) {
      Serial.println("✅ MQTT publish success");
    } else {
      Serial.println("❌ MQTT publish failed");
    }
  } else {
    updateConfigThrougMqtt(msg);
  }
}
void updateConfigThrougMqtt(String message) {
  Serial.println("Received message: " + message);

  // Parse the incoming JSON message
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.println("Failed to parse JSON message");
    return;
  }

  String action = doc["action"];
  String deviceSerial = doc["serialNumber"];

  // Check if the message is meant for this device
  if (device_serial_number == deviceSerial) {
    if (action == "UPDATE_CONFIG") {
      // Update the config file
      JsonObject configCloudServer = doc["config"];


      for (JsonPair kv : configCloudServer) {
        const char* key = kv.key().c_str();  // Get the key
        JsonVariant value = kv.value();      // Get the value

        // Do something with the key-value pair
        Serial.print("Key: ");
        Serial.print(key);
        Serial.print(", Value: ");
        Serial.println(value.as<String>());
        updateJsonConfig("config.json", key, value);
        if (String(key).startsWith("relay")) {
          int relayNum = String(key).substring(5).toInt();  // extract number after "relay"
          if (relayNum >= 0 && relayNum < 4) {
            updateRelayStatusAction(relayNum, value);
          }
        }
      }
    }
  }


  readConfig("config.json");
}


void connectToMQTT() {
  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(MqttCallback);
  if (!mqttclient.connected()) {
    //String clientId = String(device_serial_number);
    if (mqttclient.connect(clientId.c_str())) {
      Serial.println("MQTT connected");
      mqttclient.subscribe(MqttTopic_sub.c_str());
      Serial.println("Subscribed to: " + MqttTopic_sub);
    } else {
      Serial.print(mqtt_server);
      Serial.print(mqtt_port);


      Serial.print("MQTT connect failed. State: ");
      Serial.println(mqttclient.state());
      delay(2000);
    }
  }
}

void mqttsetup() {
  // mqtt_server =  config["mqtt_server"];//"broker.hivemq.com";
  // mqtt_port = config["mqtt_port"];//1883;

  mqtt_server = config["mqtt_server"].as<const char*>();
  mqtt_port = config["mqtt_port"].as<int>();
  clientId = config["mqtt_clientId"].as<String>();

  connectToMQTT();
  mqttclient.setBufferSize(2048);
}

void mqttloop() {
  if (!mqttclient.connected()) {
    connectToMQTT();
  }
  mqttclient.loop();
}

void mqttHeartBeat(String hbPayload) {
  // mqttclient.publish(topic_heartbeat, hbPayload.c_str());
  mqttclient.publish(MqttTopic_pubheartbeat.c_str(), hbPayload.c_str());
}
void mqttAlarmNotification(String hbPayload) {
  mqttclient.publish(MqttTopic_pub.c_str(), hbPayload.c_str());
}
