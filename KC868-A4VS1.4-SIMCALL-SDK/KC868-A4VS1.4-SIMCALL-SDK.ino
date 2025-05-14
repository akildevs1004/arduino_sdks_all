#define SIM7600_TX 13  // TX2 on KC868-A4S
#define SIM7600_RX 15  // RX2 on KC868-A4S

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
  delay(2000);  // Wait for SIM7600 to initialize

  Serial.println("Initializing SIM7600...");
  
  // Check if the module is responding
  Serial2.println("AT");
  delay(1000);
  if (Serial2.available()) {
    String response = Serial2.readString();
    Serial.println(response);
  }

  // Dial a phone number (replace with the desired number)
  Serial.println("Calling +971552205149...");
  Serial2.println("ATD+971552205149;");  // ATD = AT Dial
  delay(1000);
  
  // Check call status
  Serial2.println("AT+CLCC");  // List current calls
  delay(1000);
  if (Serial2.available()) {
    String callStatus = Serial2.readString();
    Serial.println(callStatus);
  }
}

void loop() {
  // Forward Serial (PC) commands to SIM7600
  if (Serial.available()) {
    Serial2.write(Serial.read());
  }

  // Forward SIM7600 responses to Serial (PC)
  if (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}