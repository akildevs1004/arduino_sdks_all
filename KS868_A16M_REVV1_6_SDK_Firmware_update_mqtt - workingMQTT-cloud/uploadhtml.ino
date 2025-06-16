

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
    // Initialize the file on the LittleFS filesystem
    String filename = "/" + upload.filename;
    fsUploadFile = LittleFS.open(filename, "w");

    // Send response to client
    Serial.printf("Upload Start: %s\n", upload.filename.c_str());
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "Uploading...");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write the uploaded chunk to LittleFS
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    // Close the file when upload finishes
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    Serial.printf("Upload End: %s (%u)\n", upload.filename.c_str(), upload.totalSize);
    server.send(200, "text/plain", "File Uploaded Successfully");
  }
}
 

void uploadHTMLsetup() {

  // Serve the file upload page
  server.on("/uploadhtmlfiles", HTTP_GET, handleUploadForm);
  server.on(
    "/uploadhtmlfiles", HTTP_POST, []() {
      server.send(200);  // End the POST response
    },
    handleHtmlFileUpload);
}
