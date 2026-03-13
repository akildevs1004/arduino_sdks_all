

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
String clientId = "xtremesosdevice";

// Device unique ID (serial number)
// const char* device_serial = "XT123456";

// MQTT topics (based on serial)
String MqttTopic_sub = "";           //clientId + "/" + device_serial_number + "/config/request";
String MqttTopic_pub = "";           //clientId + "/" + device_serial_number + "/config";
String MqttTopic_pubheartbeat = "";  //clientId + "/" + device_serial_number + "/heartbeat";
String MqttTopic_SOSalarm = "";      //clientId + "/" + device_serial_number + "/sosalarm";

// std::string topic_heartbeat_str = std::string("device/") + std::string(device_serial_number.c_str()) + "/heartbeat";

// const char* topic_heartbeat = topic_heartbeat_str.c_str();
unsigned long mqttDisconnectedSince = 0;
const unsigned long mqttRestartTimeout = 1000*60*5;  // 1 minute


WiFiClient espClient;
PubSubClient mqttclient(espClient);
StaticJsonDocument<256> mqttconfig;

unsigned long lastMqttReconnectAttempt = 0;
const long mqttReconnectInterval = 1000 * 60;  // 5 seconds

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
    publishConfigToMQTT();
  } else {
    updateConfigThrougMqtt(msg);
  }

  //heartbeat

  handleHeartbeat();
}

void publishConfigToMQTT() {

  readConfig("config.json");

  mqttclient.setBufferSize(3000);
  DynamicJsonDocument mqttconfig(3000);  // Increased from 1024 for safety
  mqttconfig["serialNumber"] = device_serial_number;
  mqttconfig["type"] = "config";
  mqttconfig["timestamp"] = millis();
  mqttconfig["config"] = deviceConfigContent;




  // // 3. Handle the config file carefully
  // String configContent = readConfig("config.json");
  // if (configContent.length() > 0) {
  //   // Option A: Add as raw string (if it's already JSON)
  //   // mqttconfig["config"] = serialized(configContent);

  //   // Option B: Parse and embed as nested JSON (better)
  //   DynamicJsonDocument configDoc(1024);
  //   DeserializationError error = deserializeJson(configDoc, configContent);
  //   if (!error) {
  //     mqttconfig["config"] = configDoc;
  //   } else {
  //     Serial.println("Failed to parse config.json: " + String(error.c_str()));
  //     mqttconfig["config"] = "invalid";
  //   }
  // } else {
  //   mqttconfig["config"] = "missing";
  // }

  // 4. Serialize and publish
  String payload;
  if (serializeJson(mqttconfig, payload) == 0) {
    Serial.println("❌ Failed to serialize JSON");
    return;
  }

  // 5. Publish with error checking
  bool sent = mqttclient.publish(MqttTopic_pub.c_str(), payload.c_str());

  mqttconfig.clear();

  // 6. Debug output
  if (sent) {
    Serial.println("✅ MQTT publish success");
    Serial.println("Payload size: " + String(payload.length()) + " bytes");
#ifdef DEBUG
    serializeJsonPretty(mqttconfig, Serial);  // Pretty-print for debugging
#endif
  } else {
    Serial.println("❌ MQTT publish failed");

    connectToMQTT();
    // Serial.println("Possible reasons:");
    // Serial.println("- MQTT not connected");
    // Serial.println("- Payload too large (" + String(payload.length()) + " bytes)");
    // Serial.println("- Network issues");
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
        Serial.print("Key param-----------------------------------: ");
        Serial.print(key);
        Serial.println(", Value: ");
        Serial.print(value.as<String>());
        updateJsonConfig("config.json", key, value);
        if (String(key).startsWith("relay")) {
          int relayNum = String(key).substring(5).toInt();  // extract number after "relay"
          if (relayNum >= 0 && relayNum < 4) {
            // updateRelayStatusAction(relayNum, value);
          }
        }
      }
    }

    publishConfigToMQTT();
  }


  readConfig("config.json");
}


void connectToMQTT() {
  if (mqttclient.connected()) return;


  

  unsigned long now = millis();
  if (now - lastMqttReconnectAttempt < mqttReconnectInterval) {
    return;  // wait before retry
  }
  lastMqttReconnectAttempt = now;

  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(MqttCallback);

  String connectId = clientId + "-" + device_serial_number;

  Serial.print("Attempting MQTT connection... ");
  Serial.print(mqtt_server);


  

  if (mqttclient.connect(connectId.c_str()))

 


  {
    Serial.println("CONNECTED");
    mqttDisconnectedSince = 0;  // reset timer

    mqttclient.subscribe(MqttTopic_sub.c_str());
    Serial.println("Subscribed: " + MqttTopic_sub);

    updateJsonConfig("config.json", "mqtt", "online");
    publishConfigToMQTT();

  } else {

    
    Serial.print("FAILED, rc=");
    Serial.print(mqttclient.state());
    Serial.println(" → retrying in 5s");

    updateJsonConfig("config.json", "mqtt", "offline");

    digitalWrite(RELAY1_NETWORK_STATUS_PIN, LOW);
    // Start disconnect timer (only once)
    if (mqttDisconnectedSince == 0) {
      mqttDisconnectedSince = now;
    }

    if (now - mqttDisconnectedSince >= mqttRestartTimeout) {
      Serial.println("MQTT disconnected > 1 minute. Restarting device...");
      delay(200);
      ESP.restart();
    }
  }
}
/*
void mqttsetup() {
  // mqtt_server =  config["mqtt_server"];//"broker.hivemq.com";
  // mqtt_port = config["mqtt_port"];//1883;


  mqtt_server = config["mqtt_server"].as<const char*>();
  mqtt_port = config["mqtt_port"].as<int>();
  clientId = config["mqtt_clientId"].as<String>();

  MqttTopic_sub = clientId + "/" + device_serial_number + "/config/request";  //get config
  MqttTopic_pub = clientId + "/" + device_serial_number + "/config";          //update config
  MqttTopic_pubheartbeat = clientId + "/" + device_serial_number + "/heartbeat";
  MqttTopic_SOSalarm = clientId + "/" + device_serial_number + "/sosalarm";





  connectToMQTT();
  mqttclient.setBufferSize(3000);
}

void mqttloop() {
  if (!mqttclient.connected()) {
    connectToMQTT();
  }
  mqttclient.loop();
}

void mqttHeartBeat(String hbPayload) {


  if (config["mqtt"] == "offline")
    connectToMQTT();


  Serial.println(MqttTopic_pubheartbeat);

  Serial.println("MQTT - Heartbeat Sent");
  Serial.println(hbPayload);

  mqttclient.publish(MqttTopic_pubheartbeat.c_str(), hbPayload.c_str());
}*/


void mqttsetup() {
  // mqtt_server =  config["mqtt_server"];//"broker.hivemq.com";
  // mqtt_port = config["mqtt_port"];//1883;


  mqtt_server = config["mqtt_server"].as<const char*>();
  mqtt_port = config["mqtt_port"].as<int>();
  clientId = config["mqtt_clientId"].as<String>();

  MqttTopic_sub = clientId + "/" + device_serial_number + "/config/request";
  MqttTopic_pub = clientId + "/" + device_serial_number + "/config";
  MqttTopic_pubheartbeat = clientId + "/" + device_serial_number + "/heartbeat";


  connectToMQTT();
  mqttclient.setBufferSize(3000);
}
 

void mqttloop() {


  static uint32_t lastAttempt = 0;

  if (!mqttclient.connected()) {
    uint32_t now = millis();
    if (now - lastAttempt > 1000 * 60) {  // try every 5s
      lastAttempt = now;
      connectToMQTT();  // single attempt only
    }
    return;  // don't call mqttclient.loop() when disconnected
  }

  mqttclient.loop();






  /*


  if (!mqttclient.connected()) {

    // unsigned long now = millis();

    // if (now - lastMqttReconnectAttempt > mqttReconnectInterval) {
    //   lastMqttReconnectAttempt = now;
    //   connectToMQTT();

    //   if (mqttclient.connected()) {
    //     lastMqttReconnectAttempt = 0;  // Reset on success
    //   }
    // }

  } else {
    mqttclient.loop();
  }
  */
}
// void mqttloop() {
//   if (!mqttclient.connected()) {
//     connectToMQTT();
//   }
//   mqttclient.loop();
// }

void mqttHeartBeat(String hbPayload) {
  // mqttclient.publish(topic_heartbeat, hbPayload.c_str());

  if (config["mqtt"] == "offline") {
      connectToMQTT();

  } else if (config["mqtt"] == "online") {

    Serial.println("MQTT - Heartbeat Sent");
    Serial.println(hbPayload);

    mqttclient.publish(MqttTopic_pubheartbeat.c_str(), hbPayload.c_str());
  }
}
void mqttAlarmNotification(String hbPayload) {
  Serial.println("MQTT - Alarm Sent");
  Serial.println(hbPayload);


  mqttclient.publish(MqttTopic_pub.c_str(), hbPayload.c_str());
  publishConfigToMQTT();
}
void mqttSOSAlarmNotification(String hbPayload) {
  Serial.println("MQTT - SOS Alarm Sent");
  // Serial.println(hbPayload);
  mqttclient.publish(MqttTopic_SOSalarm.c_str(), hbPayload.c_str());
}
