/*
  ESP32 RS485 Modbus (Fixed Slave ID = 4) + Temp/Humidity Reader
  + Digital Inputs (DI1/DI2) + Relay Control (Relay1/Relay2)
  + ✅ Long-Press Factory Reset using BOOT button (GPIO0)

  IMPORTANT (GPIO0 / BOOT):
  - Pressed = LOW
  - Do NOT hold the BOOT button while power-up/reset, otherwise ESP32 may enter flashing mode.
  - This code triggers factory reset ONLY after holding BOOT for 5 seconds during normal run.

  Pins (your board):
  Relay1: GPIO15
  Relay2: GPIO2
  DI1: GPIO36
  DI2: GPIO39
  RS485: RXD=35 TXD=32
  I2C: SDA=4 SCL=16
  GSM: RXD=34 TXD=13
*/

#include <ModbusMaster.h>
#include <Wire.h>
#include <Preferences.h>

// -------------------- RS485 --------------------
#define TXD_PIN 32
#define RXD_PIN 35

// -------------------- RELAYS --------------------
#define RELAY1_PIN 15
#define RELAY2_PIN 2

// Many relay boards are ACTIVE-LOW. If yours is opposite, set to 0.
#define RELAY_ACTIVE_LOW 1

// -------------------- DIGITAL INPUTS --------------------
#define DI1_PIN 36
#define DI2_PIN 39

// -------------------- I2C --------------------
#define I2C_SDA 4
#define I2C_SCL 16

// -------------------- GSM pins (kept as requested) --------------------
#define GSM_RXD_PIN 34
#define GSM_TXD_PIN 13

// -------------------- Ethernet (LAN8720) defines (kept as requested) --------------------
#define ETH_ADDR        0
#define ETH_POWER_PIN  -1
#define ETH_MDC_PIN    23
#define ETH_MDIO_PIN   18
#define ETH_TYPE       ETH_PHY_LAN8720
#define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT

// -------------------- FACTORY RESET BUTTON (BOOT) --------------------
#define RESET_PIN 0                 // ✅ your black button
#define RESET_HOLD_MS 5000          // hold 5 seconds
#define RESET_DEBOUNCE_MS 30
#define RESET_ACTIVE_LEVEL LOW      // pressed = LOW

// Modbus register start and count
static const uint16_t REG_START = 0x0000;
static const uint16_t REG_COUNT = 2;

ModbusMaster node;
Preferences prefs;

// Decode logic (same as your original)
float decodeTemperature(uint16_t raw) {
  if (raw < 10000) return raw * 0.1f;
  return -1.0f * (raw - 10000) * 0.1f;
}
float decodeHumidity(uint16_t raw) {
  return raw * 0.1f;
}

// Relay helper
static inline void relayWrite(uint8_t pin, bool on) {
#if RELAY_ACTIVE_LOW
  digitalWrite(pin, on ? LOW : HIGH);
#else
  digitalWrite(pin, on ? HIGH : LOW);
#endif
}

// No DE/RE used
void preTransmission() {}
void postTransmission() {}

bool tryReadAtId(uint8_t id, bool &usedHolding) {
  node.begin(id, Serial2);

  uint8_t res = node.readInputRegisters(REG_START, REG_COUNT);
  if (res == node.ku8MBSuccess) {
    usedHolding = false;
    return true;
  }

  res = node.readHoldingRegisters(REG_START, REG_COUNT);
  if (res == node.ku8MBSuccess) {
    usedHolding = true;
    return true;
  }

  return false;
}

// ---------- Factory Reset ----------
void factoryReset() {
  Serial.println("\n⚠ FACTORY RESET: clearing saved config...");
  prefs.begin("config", false);
  prefs.clear();      // clears all keys under "config"
  prefs.end();

  Serial.println("✅ Cleared. Restarting...");
  delay(1200);
  ESP.restart();
}

// Long press detection (non-blocking)
void checkLongPressReset() {
  static int lastStable = HIGH;
  static int lastRead   = HIGH;
  static unsigned long lastDebounceAt = 0;

  static bool pressing = false;
  static unsigned long pressStartedAt = 0;

  int reading = digitalRead(RESET_PIN);

  // debounce
  if (reading != lastRead) {
    lastDebounceAt = millis();
    lastRead = reading;
  }

  if ((millis() - lastDebounceAt) > RESET_DEBOUNCE_MS) {
    if (reading != lastStable) {
      lastStable = reading;

      if (lastStable == RESET_ACTIVE_LEVEL) {
        pressing = true;
        pressStartedAt = millis();
        Serial.println("⏳ BOOT button pressed... keep holding 5s to factory reset");
      } else {
        pressing = false;
      }
    }
  }

  if (pressing && (millis() - pressStartedAt >= RESET_HOLD_MS)) {
    factoryReset();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // BOOT button as input with internal pull-up
  pinMode(RESET_PIN, INPUT_PULLUP);

  // RS485 UART
  Serial2.begin(9600, SERIAL_8N1, RXD_PIN, TXD_PIN);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // Relays
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  relayWrite(RELAY1_PIN, false);
  relayWrite(RELAY2_PIN, false);

  // Digital Inputs (GPIO36/39 no internal pullups)
  pinMode(DI1_PIN, INPUT);
  pinMode(DI2_PIN, INPUT);

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println();
  Serial.println("=== RS485 Modbus (ID=4) + Temp/Hum + DI + Relays + Long-Press Reset ===");
  Serial.println("BOOT button GPIO0: Hold 5 seconds (while running) to factory reset.");
  Serial.println("⚠ Do NOT hold BOOT during power-up/reset (it may enter flash mode).");
  Serial.println("---------------------------------------------------------------------");
}

void loop() {
  checkLongPressReset();

  uint8_t foundId = 0;
  bool usedHolding = false;

  // ✅ Keep your working behavior: test only Slave ID 4
  for (uint8_t id = 4; id <= 4; id++) {
    Serial.print("Scanning ID: ");
    Serial.println(id);

    if (tryReadAtId(id, usedHolding)) {
      foundId = id;
      break;
    }

    delay(120);
    checkLongPressReset();
  }

  if (foundId == 0) {
    Serial.println("❌ No device found at ID=4.");
    Serial.println("Re-trying in 2 seconds...\n");

    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      checkLongPressReset();
      delay(10);
    }
    return;
  }

  Serial.println("\n✅ Device Found!");
  Serial.print("Slave ID: ");
  Serial.println(foundId);
  Serial.print("Register type used: ");
  Serial.println(usedHolding ? "HOLDING (0x03)" : "INPUT (0x04)");
  Serial.println("---------------------------------------------------------------------");

  // Continuous read loop
  while (true) {
    checkLongPressReset();

    node.begin(foundId, Serial2);

    uint8_t result = usedHolding
      ? node.readHoldingRegisters(REG_START, REG_COUNT)
      : node.readInputRegisters(REG_START, REG_COUNT);

    if (result == node.ku8MBSuccess) {
      uint16_t temperatureRaw = node.getResponseBuffer(0);
      uint16_t humidityRaw    = node.getResponseBuffer(1);

      float temperature = decodeTemperature(temperatureRaw);
      float humidity    = decodeHumidity(humidityRaw);

      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");

      Serial.print("Humidity:    ");
      Serial.print(humidity);
      Serial.println(" %RH");
    } else {
      Serial.print("Temp/Hum read failed, error code: ");
      Serial.println(result);
    }

    // Digital Inputs
    int di1 = digitalRead(DI1_PIN);
    int di2 = digitalRead(DI2_PIN);

    Serial.print("DI1(GPIO36): ");
    Serial.println(di1);

    Serial.print("DI2(GPIO39): ");
    Serial.println(di2);

    // Example relay mapping (change logic as you need)
    relayWrite(RELAY1_PIN, di1 == HIGH);
    relayWrite(RELAY2_PIN, di2 == HIGH);

    Serial.println("---------------------------------------------------------------------");

    // 5s delay but still responsive to long-press reset
    unsigned long t0 = millis();
    while (millis() - t0 < 5000) {
      checkLongPressReset();
      delay(10);
    }
  }
}
