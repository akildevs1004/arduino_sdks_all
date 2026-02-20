

File fsUploadFile;


void handleUploadForm() {

  String header = readFile("/header.html");
  String html = readFile("/uploadhtml.html");
  html = header + html;
  html = replaceHeaderContent(html);
  server.send(200, "text/html", html);
}

// Function to handle the file upload
void handleHtmlFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    Serial.printf("Upload Start: %s\n", filename.c_str());

    fsUploadFile = LittleFS.open(filename, "w");
    if (!fsUploadFile) {
      Serial.println("❌ Failed to open file for writing");
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write data to file
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
      Serial.printf("✅ Upload End: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
    }
  } 
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("❌ Upload Aborted");
    if (fsUploadFile) {
      fsUploadFile.close();
    }
  }
}



void uploadHTMLsetup() {

  // Serve the file upload page
  server.on("/uploadhtmlfiles", HTTP_GET, handleUploadForm);

  // server.on(
  //   "/uploadhtmlfiles", HTTP_POST, []() {
  //     server.sendHeader("Connection", "close");
  //     server.send(200, "text/plain", (Update.hasError()) ? "Update FAILED" : "Update OK - Rebooting...");
  //     delay(1000);
  //     ESP.restart();
  //   },
  //   []() {
  //     handleHtmlFileUpload();
  //   });

    server.on(
  "/uploadhtmlfiles", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "Update FAILED" : "Update OK - Rebooting...");
    delay(1000);
    ESP.restart();
  },
  []() {
    handleHtmlFileUpload();
  });


  // server.on(
  //   "/uploadhtmlfiles", HTTP_POST, []() {
  //     // server.send(200);  // End the POST response

  //     server.sendHeader("Connection", "close");
  //     server.send(200, "text/plain", (Update.hasError()) ? "Update FAILED" : "Update OK - Rebooting...");
  //     delay(1000);
  //     ESP.restart();
  //   },
  //   handleHtmlFileUpload);
}
