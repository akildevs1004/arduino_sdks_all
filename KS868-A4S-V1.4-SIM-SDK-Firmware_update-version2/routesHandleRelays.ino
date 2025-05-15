
#include <PCF8574.h>

#include <Wire.h>




// ===== Hardware Config =====
#define I2C_RELAY_ADDR 0x24
#define I2C_DI_ADDR 0x22
#define I2C_SDA 4
#define I2C_SCL 16



// SHT30 I2C Address
#define SHT30_I2C_ADDR 0x40


// ===== Global Variables =====

PCF8574 pcf8574_RE1(I2C_RELAY_ADDR, I2C_SDA, I2C_SCL);
PCF8574 pcf8574_DI(I2C_DI_ADDR, I2C_SDA, I2C_SCL);

String networkStatus = "Disconnected";
String ipAddress = "None";
bool relayStates[4] = { false };
bool diStates[8] = { false };
bool lastDI1State = false;
float temperature = 0.0;
float humidity = 0.0;






// ===== Input Monitoring =====
void checkDI1() {
  bool currentState = (pcf8574_DI.digitalRead(0) == LOW);
  if (currentState && !lastDI1State) {
    /////////// sendSMS("ALERT: DI1 (D11) is now ACTIVE!");
  }
  lastDI1State = currentState;
}

// ===== Web Handlers =====
// void handleRelaysList() {


//   String page = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
//   page += "<title>Relay & DI Monitor</title>";
//   page += "<style>body{font-family:Arial;margin:20px;} button{padding:10px;margin:5px;}</style>";
//   page += "</head><body>";
//   page += "<h1>Relay & DI Monitor</h1>";
//   page += "<p><strong>Network:</strong> " + networkStatus + "</p>";
//   page += "<p><strong>IP:</strong> " + ipAddress + "</p>";
//   page += "<h2>Environment</h2>";
//   page += "<p>Temperature: " + String(temperature) + " °C</p>";
//   page += "<p>Humidity: " + String(humidity) + " %</p>";

//   for (int i = 0; i < 4; i++) {
//     page += "<p>Relay D" + String(i + 1) + ": ";
//     page += "<a href='/relay?num=" + String(i) + "'><button>";
//     page += relayStates[i] ? "ON" : "OFF";
//     page += "</button></a></p>";
//   }

//   page += "<h2>Digital Inputs</h2>";
//   for (int i = 0; i < 8; i++) {
//     page += "<p>DI" + String(i + 1) + ": ";
//     page += diStates[i] ? "<span style='color:red;font-weight:bold'>ACTIVE</span>" : "<span style='color:green'>INACTIVE</span>";
//     page += "</p>";
//   }

//   //page += "<script>setInterval(()=>location.reload(), 5000);</script>";
//   page += "</body></html>";

//   server.send(200, "text/html", page);
// }
void updateRelayStatus(int relayNum) {

  if (relayNum >= 0 && relayNum < 4) {
    relayStates[relayNum] = !relayStates[relayNum];
    pcf8574_RE1.digitalWrite(relayNum, relayStates[relayNum] ? LOW : HIGH);

    Serial.println(String(relayNum)+":"+relayStates[relayNum]);
    updateJsonConfig("config.json", "relay" + String(relayNum), relayStates[relayNum] ==LOW?  "false": "true");
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

  pcf8574_DI.begin();
  for (int i = 0; i < 8; i++) {
    pcf8574_DI.pinMode(i, INPUT);
  }
  Serial.println("Relay Page");
  for (int i = 0; i < 8; i++) {
    diStates[i] = (pcf8574_DI.digitalRead(i) == LOW);
    
  }

  Serial.println("Relay Page End----------------------");
}

void relayLoop() {
}
