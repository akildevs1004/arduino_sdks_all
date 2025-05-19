// #include <Wire.h>
// //#include <LiquidCrystal_I2C.h>
#include <DHT.h>
// #include <WiFi.h>
// #include <WiFiClient.h>
// #include <HTTPClient.h>
// // #include <WiFiManager.h>  // WiFi configuration UI

// Pin assignments
const int RED_LED_PIN = 2;
const int GREEN_LED_PIN = 15;
const int SWITCH1_PIN = 36;
const int SWITCH2_PIN = 39;
const int SWITCH3_PIN = 27;
const int SWITCH4_PIN = 14;
const int SWITCH5_PIN = 12;
const int DOOR_PIN = 35;
const int DHT_PIN = 13;
const int DHT_TYPE = DHT11;

DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastSwitch5Press = 0;
unsigned long lastDoorChange = 0;
bool isSystemGood = true;
bool countdownStarted = false;
unsigned long countdownStartTime = 0;
const unsigned long countdownDuration = 28000;

double TEMPERATURE_THRESHOLD = 30.0;

bool switch5Pressed = false;

// WiFiManager wifiManager;
// String serverURL = "https://backend.xtremeguard.org/api/alarm_device_status";

int doorOpen = 0;
int smokeStatus = 0;
int waterLeakage = 0;
int acPowerFailure = 0;

float temperature = 0;
float humidity = 0;

float lastSentTemperature = -100.0;
unsigned long lastTempSendTime = 0;
const unsigned long TEMP_SEND_INTERVAL = 15UL * 60UL * 1000UL;
const float TEMP_CHANGE_THRESHOLD = 0.2;

// #define DEVICE_SERIAL_NUMBER "24000002"

void devicePinDefinationSetup() {
  //Serial.begin(115200);
  delay(1000);
  Serial.println("System Booting...");

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  pinMode(SWITCH1_PIN, INPUT_PULLUP);
  pinMode(SWITCH2_PIN, INPUT_PULLUP);
  pinMode(SWITCH3_PIN, INPUT_PULLUP);
  pinMode(SWITCH4_PIN, INPUT_PULLUP);
  pinMode(SWITCH5_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);

  dht.begin();

  WiFiManagerParameter customThreshold("threshold", "Temperature Threshold", String(TEMPERATURE_THRESHOLD).c_str(), 5);
  wifiManager.addParameter(&customThreshold);
  wifiManager.autoConnect("XtremeGuard");

  TEMPERATURE_THRESHOLD = atof(customThreshold.getValue());
  Serial.println("WiFi Connected. Temperature threshold set to: " + String(TEMPERATURE_THRESHOLD) + "°C");
}

void deviceReadSensorsLoop() {
  int switch1 = digitalRead(SWITCH1_PIN);
  int switch2 = digitalRead(SWITCH2_PIN);
  int switch3 = digitalRead(SWITCH3_PIN);
  int switch4 = digitalRead(SWITCH4_PIN);
  int switch5 = !digitalRead(SWITCH5_PIN);
  int door = digitalRead(DOOR_PIN);

  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  Serial.println("Temperature: " + String(temperature) + "°C | Humidity: " + String(humidity) + "%");

  if (switch5 && millis() - lastSwitch5Press > 1000) {
    lastSwitch5Press = millis();
    Serial.println("Switch 5 pressed.");
    if (digitalRead(RED_LED_PIN) == LOW && isSystemGood) {
      Serial.println("Turning RED LED ON manually");
      digitalWrite(RED_LED_PIN, HIGH);
      sendDataToServer2();
    } else {
      Serial.println("Turning RED LED OFF manually");
      digitalWrite(RED_LED_PIN, LOW);
      sendDataToServer2();
      if (temperature >= TEMPERATURE_THRESHOLD || !isSystemGood) {
        delay(30000);
      }
    }
  }

  if (switch1 || switch2 || switch3 || switch4) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    if (isSystemGood) {
      Serial.println("System Alert Detected!");
      sendDataToServer2();
    }
    isSystemGood = false;
  } else {
    if (!isSystemGood) {
      Serial.println("System returned to Good state.");
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      sendDataToServer2();
    }
    isSystemGood = true;
  }

  if (temperature >= TEMPERATURE_THRESHOLD) {
    digitalWrite(RED_LED_PIN, HIGH);
  } else {
    digitalWrite(RED_LED_PIN, LOW);
  }

  float tempDiff = abs(temperature - lastSentTemperature);
  unsigned long now = millis();

  if (tempDiff >= TEMP_CHANGE_THRESHOLD) {
    Serial.println("Temperature changed by ≥ 0.2°C. Sending update.");
    sendDataToServer2();
    lastSentTemperature = temperature;
    lastTempSendTime = now;
  } else if (now - lastTempSendTime >= TEMP_SEND_INTERVAL) {
    Serial.println("15 minutes passed. Sending periodic update.");
    sendDataToServer2();
    lastSentTemperature = temperature;
    lastTempSendTime = now;
  }

  if (switch5) {
    switch5Pressed = true;
  } else {
    switch5Pressed = false;
  }

  if (switch5Pressed) {
    digitalWrite(RED_LED_PIN, LOW);
    Serial.println("RED LED manually turned OFF via Switch 5");
    delay(1000);
  }

  if (door != lastDoorChange) {
    lastDoorChange = door;
    if (door == HIGH) {
      countdownStarted = true;
      countdownStartTime = millis();
      Serial.println("Door opened. Countdown started.");
    } else {
      if (countdownStarted) {
        Serial.println("Door closed before countdown ended.");
        countdownStarted = false;
        digitalWrite(RED_LED_PIN, LOW);
        sendDataToServer2();
      }
    }
  }

  if (countdownStarted) {
    unsigned long elapsedTime = millis() - countdownStartTime;
    unsigned long remainingTime = countdownDuration - elapsedTime;
    updateLCDCountdown(remainingTime);
    if (elapsedTime >= countdownDuration) {
      Serial.println("Door countdown completed. Triggering RED LED.");
      digitalWrite(RED_LED_PIN, HIGH);
      sendDataToServer2();
    }
  }

  delay(1000);
}

void readAllStatus() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  int switch2 = digitalRead(SWITCH2_PIN);
  int switch3 = digitalRead(SWITCH3_PIN);
  int switch4 = digitalRead(SWITCH4_PIN);
  int door = digitalRead(DOOR_PIN);

  doorOpen = (door == HIGH) ? 1 : 0;
  smokeStatus = (switch2 == HIGH) ? 1 : 0;
  waterLeakage = (switch3 == HIGH) ? 1 : 0;
  acPowerFailure = (switch4 == HIGH) ? 1 : 0;

  Serial.println("Door: " + String(doorOpen) + " | Smoke: " + String(smokeStatus) + " | Water: " + String(waterLeakage) + " | Power: " + String(acPowerFailure));
}

void sendDataToServer2() {
  readAllStatus();

  HTTPClient http;
  String jsonData = "{";
  jsonData += "\"serialNumber\": \"" + String(device_serial_number) + "\"";
  jsonData += ",\"humidity\": \"" + String(humidity) + "\"";
  jsonData += ",\"temperature\": \"" + String(temperature) + "\"";
  jsonData += ",\"doorOpen\": \"" + String(doorOpen) + "\"";
  jsonData += ",\"smokeStatus\": \"" + String(smokeStatus) + "\"";
  jsonData += ",\"waterLeakage\": \"" + String(waterLeakage) + "\"";
  jsonData += ",\"acPowerFailure\": \"" + String(acPowerFailure) + "\"";


  if (temperature >= TEMPERATURE_THRESHOLD) {

    jsonData += ",\"temperature_alarm\": \"1\"";
  }

  jsonData += "}";

  Serial.println("Sending JSON: " + jsonData);

  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode > 0) {
    Serial.println("Server Response Code: " + String(httpResponseCode));
  } else {
    Serial.println("HTTP POST Failed. Error: " + String(httpResponseCode));
  }

  http.end();
}

void updateLCDCountdown(unsigned long remainingTime) {
  // Optionally print to Serial for now
  Serial.println("Countdown: " + String(remainingTime / 1000) + "s remaining");
}