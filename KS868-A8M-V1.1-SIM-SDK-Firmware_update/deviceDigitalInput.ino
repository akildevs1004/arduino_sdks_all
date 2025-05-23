#include <Wire.h>
#include <PCF8574.h>

// ====== I2C Configuration ======
#define I2C_DI_ADDR 0x22
#define I2C_SDA 5
#define I2C_SCL 4


PCF8574 pcf8574_DI(I2C_DI_ADDR, I2C_SDA, I2C_SCL);

#define DI_COUNT 7

// ====== Digital Input States ======
bool diStates[DI_COUNT] = { false };
bool lastDIStates[DI_COUNT] = { false };

unsigned long doorOpenStartTime = 0;
bool doorWasOpen = false;
bool buzzerTriggeredForDoor = false;
bool isAAnylarmOn = false;




// ===== Hardware Config =====
#define I2C_RELAY_ADDR 0x24
#define I2C_SDA 5
#define I2C_SCL 4
// SHT30 I2C Address
#define SHT30_I2C_ADDR 0x40

#define RELAY_FAN 0
#define RELAY_AC 1
#define RELAY_LAMP 2
#define RELAY_BUZZER 3
#define RELAY_LED 4


#define DI_FIRE 0
#define DI_WATER 1
#define DI_AC_POWER 2
#define DI_DOOR 3
#define DI_SMOKE 4
#define DI_PAUSE_BUZZER 5
#define DI_FACTORY_RESET 6

bool factoryResetTriggered = false;
unsigned long factoryResetStartTime = 0;
// #define DI_PENDING 7

//Relay Defination
//#1 - AC
//#2 - FAN
//#3 - LAMP
//#4 - SIREN
//#5 - LED
//#6 -
//#7 -
//#8 -

bool buzzerPaused = false;

unsigned long buzzerPauseStartTime = 0;

// ===== Global Variables =====
int max_siren_pause = 15;
PCF8574 pcf8574_RE1(I2C_RELAY_ADDR, I2C_SDA, I2C_SCL);
#define RELAY_COUNT 8

bool relayStates[RELAY_COUNT] = { false };

//DI Defination
//#1 - Fire
//#2 - Water
//#3 - A/C Power
//#4 - Door
//#5 - Smoke
//#6 - Pause Buzzer
//#7 - Factory Reset
//#8 -










bool fire_alarm = false;
bool waterLeakage = false;
bool acPowerFailure = false;
bool doorOpen = false;
bool smoke_alarm = false;




// ====== Setup ======
void digitalSetup() {

  Serial.println("-------------DIGITAL INPUT -----------------");

  Wire.begin(I2C_SDA, I2C_SCL);

  pcf8574_DI.begin();
  for (int i = 0; i < DI_COUNT; i++) {
    pcf8574_DI.pinMode(i, INPUT);
    lastDIStates[i] = (pcf8574_DI.digitalRead(i) == HIGH);  // Initialize with current state
  }

  Serial.println("DI Monitoring Setup Done");
}

// ====== Monitor Inputs ======


bool checkAnyAlarmOpen() {
  isAAnylarmOn = false;
  for (int i = 0; i < DI_COUNT; i++) {

    if (lastDIStates[i]) {
      isAAnylarmOn = true;
    }
  }

  return isAAnylarmOn;
}

void updateLatestAlarmStatus() {
  StaticJsonDocument<64> doc;
  String jsonTempData = "";  // Clear each time

  doc["serialNumber"] = device_serial_number;
  doc["type"] = "alarm";
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["fire_alarm"] = 0;      //fire_alarm ? 1 : 0;
  doc["waterLeakage"] = 0;    // waterLeakage ? 1 : 0;
  doc["acPowerFailure"] = 0;  // acPowerFailure ? 1 : 0;
  doc["doorOpen"] = 0;        // doorOpen ? 1 : 0;
  doc["smoke_alarm"] = 0;     //smoke_alarm ? 1 : 0;
  serializeJson(doc, jsonTempData);

  Serial.println("Sending: " + jsonTempData);
  if (config["http_trigger_alarm"]) {
    sendTemperatureDataToServerHttp(jsonTempData);
  }
  sendAlarmTriggerToSocketserver(jsonTempData);
}
void checkAllDI() {

  Serial.println("-------------DIGITAL INPUT LOOP-----------------");

  for (int i = 0; i < DI_COUNT; i++) {
    bool currentState = (pcf8574_DI.digitalRead(i) == HIGH);  // Active HIGH
    // Serial.println("Sensor " + String(i) + " - " + (currentState ? "On" : "Off") + " - Prev " + (lastDIStates[i] ? "On" : "Off"));


    if (currentState != lastDIStates[i]) {
      Serial.println("Sensor " + String(i) + " is " + (currentState ? "On" : "Off"));
      lastDIStates[i] = currentState;

      StaticJsonDocument<64> doc;
      String jsonTempData = "";  // Clear each time

      doc["serialNumber"] = device_serial_number;
      doc["digital_serial_number"] = i;
      doc["type"] = "alarm";

      doc["temperature"] = temperature;

      doc["humidity"] = humidity;




      // Add field based on pin number
      // Update flags and JSON based on DI index
      if (i == DI_FIRE) {
        fire_alarm = currentState;
        doc["fire_alarm"] = fire_alarm ? 1 : 0;
      } else if (i == DI_WATER) {
        waterLeakage = currentState;
        doc["waterLeakage"] = waterLeakage ? 1 : 0;
      } else if (i == DI_AC_POWER) {
        acPowerFailure = currentState;
        doc["acPowerFailure"] = acPowerFailure ? 1 : 0;
      } else if (i == DI_DOOR) {
        doorOpen = currentState;
        doc["doorOpen"] = doorOpen ? 1 : 0;
      } else if (i == DI_SMOKE) {
        smoke_alarm = currentState;
        doc["smoke_alarm"] = smoke_alarm ? 1 : 0;
      }



      serializeJson(doc, jsonTempData);


      if (i == DI_DOOR && currentState && config["door_checkbox"]) {

        doorOpenStartTime = millis();  // Mark the time when door opened

        callRelayBuzzerTurn(currentState);
        delay(250);
        callRelayBuzzerTurn(!currentState);
        delay(250);
        callRelayBuzzerTurn(currentState);
        delay(250);
        callRelayBuzzerTurn(!currentState);
        doorWasOpen = true;

        cheKDoorKeepOpenStatus(currentState);

      } else if (i == DI_PAUSE_BUZZER && currentState) {  //pause siren buzzer

        pauseBuzzerFor5Min();

      } else {

        if ((i == DI_FIRE && config["fire_checkbox"]) || (i == DI_WATER && config["water_checkbox"]) || (i == DI_AC_POWER && config["power_checkbox"]) || (i == DI_DOOR && config["door_checkbox"]) || (i == DI_SMOKE && config["smoke_checkbox"])) {
          callRelayBuzzerTurn(currentState);
          Serial.println("Sending: " + jsonTempData);
          if (config["http_trigger_alarm"]) {
            sendTemperatureDataToServerHttp(jsonTempData);
          }
          sendAlarmTriggerToSocketserver(jsonTempData);
        }
      }
    }
    Serial.println(String("DI: ") + String(i));
    // if (i == DI_FACTORY_RESET && currentState) {
    //   restoreDefaultConfig();
    // }

    // Factory reset logic - hold for 3 to 5 seconds
    if (i == DI_FACTORY_RESET) {

      if (currentState) {

        Serial.println(String("Reset Time ") + String(millis() - factoryResetStartTime));
        if ( !factoryResetTriggered) {
          factoryResetStartTime = millis();
          factoryResetTriggered = true;
        } else if (millis() - factoryResetStartTime >= 1000 * 5 && millis() - factoryResetStartTime <= 1000 * 20) {
          restoreDefaultConfig();
          factoryResetTriggered = false;
        }
      } else {
        factoryResetTriggered = false;
      }
    }

    if (i == DI_DOOR && currentState && config["door_checkbox"]) {  //door

      cheKDoorKeepOpenStatus(currentState);

    } else if (i == DI_DOOR && !currentState && config["door_checkbox"])  //door closed
    {
      buzzerTriggeredForDoor = false;
    }
  }
}
void cheKDoorKeepOpenStatus(bool currentState) {

  Serial.println(String("Door   - ") + (currentState ? "Open" : "Closed") + String(millis() - doorOpenStartTime));


  int doorOpenDurationTime = 3;
  if (config["max_doorcontact"]) {
    doorOpenDurationTime = config["max_doorcontact"].as<int>();
  }

  if (doorOpenDurationTime == 0) doorOpenDurationTime = 3;



  // Door sensor
  // Door sensor
  if (currentState) {  // Door is open

    Serial.println(String(" Door has been open for over buzzerTriggeredForDoor: ") + String(buzzerTriggeredForDoor) + "- Minuntes " + String(doorOpenDurationTime) + "- Checking Delay " + String(millis() - doorOpenStartTime >= 1000 * 60 * doorOpenDurationTime));


    // Door has been open for some time
    if ((millis() - doorOpenStartTime >= 1000 * 60 * doorOpenDurationTime) && !buzzerTriggeredForDoor) {  // 3 minutes
      //Serial.println(String("🚨 Door has been open for over  ") + String(doorOpenDurationTime) + String(" minutes! Triggering buzzer."));
      callRelayBuzzerTurn(true);
      buzzerTriggeredForDoor = true;


      StaticJsonDocument<64> doc;
      String jsonTempData = "";  // Clear each time

      doc["serialNumber"] = device_serial_number;
      doc["digital_serial_number"] = 3;
      doc["type"] = "alarm";
      doc["temperature"] = temperature;
      doc["humidity"] = humidity;
      doc["doorOpen"] = 1;

      serializeJson(doc, jsonTempData);
      Serial.println("Sending: " + jsonTempData);
      // Add field based on pin number

      doc["doorOpen"] = currentState ? 1 : 0;

      if (config["http_trigger_alarm"]) {
        sendTemperatureDataToServerHttp(jsonTempData);
      }
      sendAlarmTriggerToSocketserver(jsonTempData);
    }
  } else {  // Door is closed
    doorOpenStartTime = 0;
    buzzerTriggeredForDoor = false;
    callRelayBuzzerTurn(false);
  }
}

// ====== Loop ======
void digitalLoop() {
  checkAllDI();
  delay(1000);  // Polling delay, adjust as needed
}
//-------------------------------------------------------------RELAY
void updateRelayStatusAction(int relayNum, bool status) {

  Serial.println(String(relayNum) + ":" + String(status));


  if (relayNum >= 0 && relayNum < RELAY_COUNT) {

    pcf8574_RE1.digitalWrite(relayNum, status ? LOW : HIGH);

    updateJsonConfig("config.json", "relay" + String(relayNum), status ? "true" : "false");
  }
}
void updateRelayStatus(int relayNum) {

  if (relayNum >= 0 && relayNum < RELAY_COUNT) {
    relayStates[relayNum] = !relayStates[relayNum];
    pcf8574_RE1.digitalWrite(relayNum, relayStates[relayNum] ? LOW : HIGH);

    Serial.println(String(relayNum) + ":" + relayStates[relayNum]);


    updateJsonConfig("config.json", "relay" + String(relayNum), relayStates[relayNum] == LOW ? "false" : "true");
  }
}
void handleRelayControl() {
  int relayNum = server.arg("num").toInt();
  updateRelayStatus(relayNum);
}

// ===== Setup =====
void relaysSetup() {

  Wire.begin(I2C_SDA, I2C_SCL);



  pcf8574_RE1.begin();
  for (int i = 0; i < RELAY_COUNT; i++) {
    pcf8574_RE1.pinMode(i, OUTPUT);
    pcf8574_RE1.digitalWrite(i, HIGH);
    //updating initial values
    updateJsonConfig("config.json", "relay" + String(i), "false");
  }

  Serial.println("Relay Page End----------------------");


  if (config["max_siren_pause"]) {
    max_siren_pause = config["max_siren_pause"].as<int>();
  }

  delay(1000);
}

void relayLoop() {
  Serial.print("max_siren_pause ");

  Serial.print(max_siren_pause);
  Serial.print(" - isALarm ");

  Serial.println(checkAnyAlarmOpen());

  Serial.println(String("Buzzer   - Alarm ") + (checkAnyAlarmOpen() ? "Open" : "Closed") + String(millis() - buzzerPauseStartTime));

  if (buzzerPaused && millis() - buzzerPauseStartTime >= 1000 * 60 * max_siren_pause && checkAnyAlarmOpen()) {
    buzzerPaused = false;
    Serial.println("🔔 Buzzer pause expired — Buzzer re-enabled.");
    buzzerTriggeredForDoor = false;
    buzzerPauseStartTime = 0;

    callRelayBuzzerTurn(true);  // Make sure buzzer is OFF

    StaticJsonDocument<64> doc;
    String jsonTempData = "";  // Clear each time

    doc["serialNumber"] = device_serial_number;
    doc["digital_serial_number"] = 5;
    doc["type"] = "alarm";
    doc["doorOpen"] = false;

    doc["buzzer_paused"] = false;
    doc["buzzer_notes"] = "buzzer_pause_expired";



    if (config["http_trigger_alarm"]) {
      sendTemperatureDataToServerHttp(jsonTempData);
    }
    sendAlarmTriggerToSocketserver(jsonTempData);
  }
}

void pauseBuzzerFor5Min() {

  buzzerPaused = true;
  buzzerPauseStartTime = millis();
  callRelayBuzzerTurn(false);  // Make sure buzzer is OFF
  Serial.println("🔕 Buzzer paused for 5 minutes.");

  StaticJsonDocument<64> doc;
  String jsonTempData = "";  // Clear each time

  doc["serialNumber"] = device_serial_number;
  doc["digital_serial_number"] = 5;
  doc["type"] = "alarm";

  doc["buzzer_paused"] = true;
  doc["buzzer_notes"] = "buzzer_pause_button_activated";



  if (config["http_trigger_alarm"]) {
    sendTemperatureDataToServerHttp(jsonTempData);
  }
  sendAlarmTriggerToSocketserver(jsonTempData);
}

void callRelayBuzzerTurn(bool buzzerShouldBeOn) {

  if (config["siren_checkbox"]) {


    // if (config["relay" + String(RELAY_BUZZER)] != buzzerShouldBeOn)
    {
      Serial.println(String("New Buzzer - ") + (buzzerShouldBeOn ? "Buzzer On" : "Buzzer Off"));


      pcf8574_RE1.digitalWrite(RELAY_BUZZER, buzzerShouldBeOn ? LOW : HIGH);  // LOW = ON
      pcf8574_RE1.digitalWrite(RELAY_LED, buzzerShouldBeOn ? LOW : HIGH);     // LOW = ON

      updateJsonConfig("config.json", "relay" + String(RELAY_LED), buzzerShouldBeOn ? "true" : "false");

      updateJsonConfig("config.json", "relay" + String(RELAY_BUZZER), buzzerShouldBeOn ? "true" : "false");
    }
  }
}
