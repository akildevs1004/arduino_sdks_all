
#include <PCF8574.h>
#include <Wire.h>




// ===== Hardware Config =====
#define I2C_RELAY_ADDR 0x24

#define I2C_SDA 5
#define I2C_SCL 4



// SHT30 I2C Address
#define SHT30_I2C_ADDR 0x40


// ===== Global Variables =====

PCF8574 pcf8574_RE1(I2C_RELAY_ADDR, I2C_SDA, I2C_SCL);
 


// String networkStatus = "Disconnected";
// String ipAddress = "None";
bool relayStates[4] = { false };
// bool diStates[8] = { false };
// bool lastDI1State = false;


// // ===== Input Monitoring =====
// void checkDI1(int i) {
//   bool currentState = (pcf8574_DI.digitalRead(i) == LOW);
//   if (currentState && !lastDI1State) {
//     Serial.println("Sensor " + i + " " + currentState ? "Off" : "On");
//   }
//   lastDI1State = currentState;
// }
 
void updateRelayStatusAction(int relayNum, bool status) {

  Serial.println(String(relayNum) + ":" + String(status));


  if (relayNum >= 0 && relayNum < 4) {

    pcf8574_RE1.digitalWrite(relayNum, status ? LOW : HIGH);

    updateJsonConfig("config.json", "relay" + String(relayNum), status ? "true" : "false");
  }
}
void updateRelayStatus(int relayNum) {

  if (relayNum >= 0 && relayNum < 4) {
    relayStates[relayNum] = !relayStates[relayNum];
    pcf8574_RE1.digitalWrite(relayNum, relayStates[relayNum] ? LOW : HIGH);

    Serial.println(String(relayNum) + ":" + relayStates[relayNum]);


    updateJsonConfig("config.json", "relay" + String(relayNum), relayStates[relayNum] == LOW ? "false" : "true");
  }
}
void handleRelayControl() {
  int relayNum = server.arg("num").toInt();
  updateRelayStatus(relayNum);
  // if (relayNum >= 0 && relayNum < 4) {
  //   relayStates[relayNum] = !relayStates[relayNum];
  //   pcf8574_RE1.digitalWrite(relayNum, relayStates[relayNum] ? LOW : HIGH);

  //   updateJsonConfig("config.json", "relay" + String(relayNum), String(relayNum));


  //   // server.sendHeader("Location", "/relayslist");
  //   // server.send(303);
  // }
}

// ===== Setup =====
void relaysSetup() {

  Wire.begin(I2C_SDA, I2C_SCL);



  pcf8574_RE1.begin();
  for (int i = 0; i < 4; i++) {
    pcf8574_RE1.pinMode(i, OUTPUT);
    pcf8574_RE1.digitalWrite(i, HIGH);
    //updating initial values
    updateJsonConfig("config.json", "relay" + String(i), "false");
  }

  // pcf8574_DI.begin();
  // for (int i = 0; i < 8; i++) {
  //   pcf8574_DI.pinMode(i, INPUT);
  // }
  // Serial.println("Relay Page");
  // for (int i = 0; i < 8; i++) {
  //   diStates[i] = (pcf8574_DI.digitalRead(i) == LOW);
  // }

  Serial.println("Relay Page End----------------------");
}

void relayLoop() {

 
}
