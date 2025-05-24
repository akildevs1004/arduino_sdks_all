// Progress tracking
int progress = 0;
unsigned long totalSize = 0;
unsigned long uploadedSize = 0;

void updateFirmWaresetup() {
  Serial.println("---------------------------------------------updateFirmWaresetup--------------------------");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  // if (!MDNS.begin("esp32")) {
  //   Serial.println("Error setting up MDNS responder!");
  //   while (1) {
  //     delay(1000);
  //   }
  // }
  // Setup OTA
  // ArduinoOTA.begin();

  // Route for root / web page
  server.on("/updatefirmware", HTTP_GET, []() {
    String header = readFile("/header.html");

    String html = readFile("/updatefirmware.html");

    html = header + html;

    html = replaceHeaderContent(html);

    server.send(200, "text/html", html);
  });

  // Handling uploading firmware file
  server.on(
    "/updatefirmwareSubmit", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "Update FAILED" : "Update OK - Rebooting...");
      delay(1000);
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update started: %s\n", upload.filename.c_str());
        uploadedSize = 0;

        // Get file size from header if available
        if (server.hasHeader("Content-Length")) {
          totalSize = atol(server.header("Content-Length").c_str());
          Serial.printf("File size: %lu bytes\n", totalSize);
        } else {
          totalSize = 0;
        }

        // Start update
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write chunk
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }

        // Update progress
        uploadedSize += upload.currentSize;
        if (totalSize > 0) {
          progress = (uploadedSize * 100) / totalSize;
          Serial.printf("Progress: %d%%\n", progress);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });
}

void updateFirmWareLoop() {

  ArduinoOTA.handle();
}