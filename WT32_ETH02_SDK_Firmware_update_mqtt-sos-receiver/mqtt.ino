

// #define MQTT_MAX_PACKET_SIZE 2048  // or 1024, or more
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>


// WiFi credentials
// const char* ssid = "akil";          // Enter your WiFi name
// const char* password = "Akil1234";  // Enter WiFi password

// MQTT Broker
String mqtt_server = "broker.hivemq.com";
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
const unsigned long mqttRestartTimeout = 1000 * 60 * 5;  // 1 minute
volatile bool mqttSendConfigRequested = false;

bool skipReconnectInterval = false;

WiFiClient espClient;
PubSubClient mqttclient(espClient);
StaticJsonDocument<256> mqttconfig;

unsigned long lastMqttReconnectAttempt = 0;
const long mqttReconnectInterval = 1000 * 30;  // 5 seconds

// std::string topic_heartbeat_str = std::string("device/") + String(device_serial_number) + "/heartbeat";
// const char* topic_heartbeat = topic_heartbeat_str.c_str();




unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 10000;  // 10 seconds


void MqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);

  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.printf("Received [%s]: %s\n", topic, msg.c_str());

  if (msg == "GET_CONFIG") {
    // Don't publish directly here
    // mqttSendConfigRequested = true;
    publishConfigToMQTT();
  } else {
    updateConfigThrougMqtt(msg);
  }
}
void handleMqttTasks() {
  if (mqttSendConfigRequested && mqttclient.connected()) {
    mqttSendConfigRequested = false;
    publishConfigToMQTT();
  }
}
//request from cloud server
// void MqttCallback(char* topic, byte* payload, unsigned int length) {
//   payload[length] = '\0';
//   String msg = (char*)payload;
//   Serial.printf("Received [%s]: %s\n", topic, msg.c_str());



//   if (msg == "GET_CONFIG") {
//     publishConfigToMQTT();
//   } else {
//     updateConfigThrougMqtt(msg);
//   }

//   //heartbeat

//   //handleHeartbeat();
// }
void publishConfigToMQTT() {
  readConfig("config.json");

  skipReconnectInterval = true;
  connectToMQTT();

  DynamicJsonDocument mqttconfig(8192);

  mqttconfig["serialNumber"] = device_serial_number;
  mqttconfig["type"] = "config";
  mqttconfig["timestamp"] = millis();
  mqttconfig["config"] = deviceConfigContent;

  String payload;
  size_t len = serializeJson(mqttconfig, payload);

  if (len == 0) {
    // updateJsonConfig("config.json", "last_message", "Failed to serialize JSON");
    Serial.println("❌ Failed to serialize JSON");
    return;
  }

  Serial.println("----- MQTT CONFIG PUBLISH -----");
  Serial.print("Topic: ");
  Serial.println(MqttTopic_pub);
  Serial.print("Topic length: ");
  Serial.println(MqttTopic_pub.length());
  Serial.print("Payload length: ");
  Serial.println(payload.length());

  if (!mqttclient.connected()) {
    Serial.println("❌ MQTT not connected, skip publish");

    // updateJsonConfig("config.json", "last_message", "MQTT not connected, skip publish");
    skipReconnectInterval = true;
    connectToMQTT();
    //return;
  }

  bool sent = mqttclient.publish(MqttTopic_pub.c_str(), payload.c_str());

  if (sent) {
    Serial.println("✅ MQTT publish success");
    // updateJsonConfig("config.json", "last_message", "MQTT publish success");

  } else {
    Serial.println("❌ MQTT publish failed");
    // updateJsonConfig("config.json", "last_message", "MQTT publish failed");
    Serial.print("MQTT state: ");
    Serial.println(mqttclient.state());
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




  // if (mqttclient.connected() && config["mqtt"] == "online") return;
  if (mqttclient.connected()) {
    config["mqtt"] = "online";
    return;
  }


  // updateJsonConfig("config.json", "last_message", "MQTT-OFFLINE");


  unsigned long now = millis();
  // if (now - lastMqttReconnectAttempt < mqttReconnectInterval || skipReconnectInterval) {
  //   return;  // wait before retry
  // }
  lastMqttReconnectAttempt = now;
  skipReconnectInterval = false;


  mqtt_server = config["mqtt_server"].as<String>();

  // mqttclient.setServer(mqtt_server.c_str(), mqtt_port);
  // mqttclient.setCallback(MqttCallback);

  String connectId = clientId + "-" + device_serial_number;

  Serial.print("Attempting MQTT connection... ");
  Serial.print(mqtt_server);


  // mqttclient.setBufferSize(3000);
  // mqttclient.setKeepAlive(60);
  // mqttclient.setSocketTimeout(15);


  if (mqttclient.connect(connectId.c_str())) {

    Serial.println("CONNECTED");
    mqttDisconnectedSince = 0;  // reset timer

    mqttclient.subscribe(MqttTopic_sub.c_str());
    Serial.println("Subscribed: " + MqttTopic_sub);

    delay(300);
    // publishConfigToMQTT();
    updateJsonConfig("config.json", "mqtt", "online");
    // updateJsonConfig("config.json", "last_message", "MQTT-CONNECTED");

  } else {


    Serial.print("FAILED, rc=");
    Serial.print(mqttclient.state());
    Serial.println(" → retrying in 5s");

    updateJsonConfig("config.json", "mqtt", "offline");
    // updateJsonConfig("config.json", "last_message", "MQTT-FAILED");

    digitalWrite(RELAY1_NETWORK_STATUS_PIN, LOW);
    // Start disconnect timer (only once)
    if (mqttDisconnectedSince == 0) {
      mqttDisconnectedSince = now;
    }

    /*
    if (now - mqttDisconnectedSince >= mqttRestartTimeout) {
      Serial.println("MQTT disconnected > 1 minute. Restarting device...");
      delay(200);
      ESP.restart();
    }
    */
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


  mqtt_server = config["mqtt_server"].as<String>();
  mqtt_port = config["mqtt_port"].as<int>();
  clientId = config["mqtt_clientId"].as<String>();

  MqttTopic_sub = clientId + "/" + device_serial_number + "/config/request";
  MqttTopic_pub = clientId + "/" + device_serial_number + "/config";
  MqttTopic_pubheartbeat = clientId + "/" + device_serial_number + "/heartbeat";
  MqttTopic_SOSalarm = clientId + "/" + device_serial_number + "/sosalarm";


  mqttclient.setServer(mqtt_server.c_str(), mqtt_port);
  mqttclient.setCallback(MqttCallback);
  mqttclient.setBufferSize(4096);
  mqttclient.setKeepAlive(60);
  mqttclient.setSocketTimeout(15);


  connectToMQTT();
}


void mqttloop() {
  static uint32_t lastAttempt = 0;
  if (mqttclient.connected()) {
    mqttclient.loop();
  } else {
    uint32_t now = millis();
    if (now - lastAttempt > 1000 * 10) {  // try every 5s
      lastAttempt = now;
      connectToMQTT();  // single attempt only
    }
    return;  // don't call mqttclient.loop() when disconnected
  }



  // handleMqttTasks();




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
  skipReconnectInterval = true;
  connectToMQTT();
  if (config["mqtt"] == "online" && mqttclient.connected()) {
    Serial.println("MQTT - Heartbeat Sent");
    Serial.println(hbPayload);

    Serial.print("Heartbeat topic len: ");
    Serial.println(MqttTopic_pubheartbeat.length());
    Serial.print("Heartbeat payload len: ");
    Serial.println(hbPayload.length());

    bool ok = mqttclient.publish(MqttTopic_pubheartbeat.c_str(), hbPayload.c_str());

    Serial.print("Heartbeat publish result: ");
    Serial.println(ok ? "OK" : "FAIL");

    if (!ok) {
      Serial.println("Heartbeat publish failed - possible buffer size/topic size issue");
    }
  }
}
void mqttAlarmNotification(String hbPayload) {
  skipReconnectInterval = true;
  connectToMQTT();
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
