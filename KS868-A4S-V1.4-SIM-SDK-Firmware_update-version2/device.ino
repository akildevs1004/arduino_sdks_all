

#define MAX485_DE 25  // RS485 DE/RE control pin
#define RS485_TX 32   // RS485 TX pin
#define RS485_RX 33   // RS485 RX pin

ModbusMaster sensor1;  // Address 4 (changed)
ModbusMaster sensor2;  // Address 2
ModbusMaster sensor3;  // Address 3

void preTransmission() {
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_DE, 0);
}

void DeviceSetup() {
  // Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);

  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_DE, 0);

  // Initialize sensors with respective addresses
  sensor1.begin(4, Serial2);  // ✅ Changed to address 4
  sensor1.preTransmission(preTransmission);
  sensor1.postTransmission(postTransmission);

  sensor2.begin(2, Serial2);
  sensor2.preTransmission(preTransmission);
  sensor2.postTransmission(postTransmission);

  sensor3.begin(3, Serial2);
  sensor3.preTransmission(preTransmission);
  sensor3.postTransmission(postTransmission);


  delay(1000);

  relaysSetup();
}

void readSensor(ModbusMaster& node, const char* name) {
  uint8_t result;
  uint16_t tempRaw, humRaw;
  float temperature, humidity;

  result = node.readHoldingRegisters(0x0000, 2);  // Temp @ 0x0000, Hum @ 0x0001

  if (result == node.ku8MBSuccess) {
    tempRaw = node.getResponseBuffer(0);
    humRaw = node.getResponseBuffer(1);

    if (tempRaw < 10000)
      temperature = tempRaw * 0.1;
    else
      temperature = -1 * (tempRaw - 10000) * 0.1;

    humidity = humRaw * 0.1;

    Serial.print(name);
    Serial.print(" - Temp: ");
    Serial.print(temperature);
    Serial.print(" °C, Hum: ");
    Serial.print(humidity);
    Serial.println(" %");
  } else {
    Serial.print(name);
    Serial.print(" - Read Error: ");
    Serial.println(result, HEX);
  }
}

void Deviceloop() {
  readSensor(sensor1, "Sensor 1 (Addr 4)");
  delay(100);
  readSensor(sensor2, "Sensor 2 (Addr 2)");
  delay(100);
  readSensor(sensor3, "Sensor 3 (Addr 3)");
  delay(100);  // Wait before repeating

  relayLoop();
  ///networkLoop();
}