#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "akil";          // Enter your WiFi name
const char* password = "Akil1234";  // Enter WiFi password

// MQTT Broker
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;

// Device unique ID (serial number)
const char* device_serial = "XT123456";

// MQTT topics (based on serial)
String MqttTopic_sub = "device/" + String(device_serial) + "/config/request";
String MqttTopic_pub = "device/" + String(device_serial) + "/config";

WiFiClient espClient;
PubSubClient mqttclient(espClient);
StaticJsonDocument<256> config;

std::string topic_heartbeat_str = std::string("device/") + device_serial + "/heartbeat";
const char* topic_heartbeat = topic_heartbeat_str.c_str();
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 10000;  // 10 seconds




//request from cloud server
void MqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String msg = (char*)payload;
  Serial.printf("Received [%s]: %s\n", topic, msg.c_str());

  if (msg == "get_config") {
    String payload;
    serializeJson(config, payload);
    mqttclient.publish(MqttTopic_pub.c_str(), payload.c_str());
    Serial.println("Published config: " + payload);
  }
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void connectToMQTT() {
  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(MqttCallback);
  while (!mqttclient.connected()) {
    String clientId = String(device_serial);
    if (mqttclient.connect(clientId.c_str())) {
      Serial.println("MQTT connected");
      mqttclient.subscribe(MqttTopic_sub.c_str());
      Serial.println("Subscribed to: " + MqttTopic_sub);
    } else {
      Serial.print("MQTT connect failed. State: ");
      Serial.println(mqttclient.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  wifisetup();

  // Sample config
  config["device"] = device_serial;
  config["version"] = "1.0";
  config["threshold"] = 25.5;

  // connectToWiFi();

  connectToMQTT();
}

void loop() {
  if (!mqttclient.connected()) {
    connectToMQTT();
  }
  mqttclient.loop();
  unsigned long now = millis();
  if (now - lastHeartbeat >= heartbeatInterval) {
    lastHeartbeat = now;

    StaticJsonDocument<128> heartbeat;
    heartbeat["device"] = device_serial;
    heartbeat["status"] = "alive";
    heartbeat["time"] = millis() / 1000;

    String hbPayload;
    serializeJson(heartbeat, hbPayload);
    mqttclient.publish(topic_heartbeat, hbPayload.c_str());
    Serial.println("Heartbeat sent: " + hbPayload);
  }
}
