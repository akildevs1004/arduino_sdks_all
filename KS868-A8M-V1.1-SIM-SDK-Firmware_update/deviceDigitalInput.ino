#include <Wire.h>
#include <PCF8574.h>

// ====== I2C Configuration ======
#define I2C_DI_ADDR 0x22
#define I2C_SDA 5
#define I2C_SCL 4

PCF8574 pcf8574_DI(I2C_DI_ADDR, I2C_SDA, I2C_SCL);

// ====== Digital Input States ======
bool diStates[8] = { false };
bool lastDIStates[8] = { false };

// ====== Setup ======
void digitalSetup() {
  Wire.begin(I2C_SDA, I2C_SCL);

  pcf8574_DI.begin();
  for (int i = 0; i < 8; i++) {
    pcf8574_DI.pinMode(i, INPUT);
    lastDIStates[i] = (pcf8574_DI.digitalRead(i) == LOW);  // Initialize with current state
  }

  Serial.println("DI Monitoring Setup Done");
}

// ====== Monitor Inputs ======
void checkAllDI() {
  for (int i = 0; i < 8; i++) {
    bool currentState = (pcf8574_DI.digitalRead(i) == LOW);  // Active LOW
    if (currentState != lastDIStates[i]) {
      Serial.println("Sensor " + String(i) + " is " + (currentState ? "On" : "Off"));
      lastDIStates[i] = currentState;
    }
  }
}

// ====== Loop ======
void digitalLoop() {
  checkAllDI();
  delay(200);  // Polling delay, adjust as needed
}
