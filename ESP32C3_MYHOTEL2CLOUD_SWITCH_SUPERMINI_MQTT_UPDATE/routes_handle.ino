String loginErrorMessage = "";
bool loginStatus = false;
void routesSetup() {
  server.on("/", HTTP_GET, handleLoginPage);
  server.on("/form1", handleRoot);
  server.on("/save_config", HTTP_POST, handleSaveConfig);
  server.on("/config.json", HTTP_GET, handleConfigJson);  //Read Config from Software API
  server.on("/logo", HTTP_GET, handleLogoImage);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/restart", HTTP_GET, handleRestartDevice);
  server.on("/styles.css", HTTP_GET, handleCSS);
}

void handleRestartDevice() {


  server.send(200, "text/html",
              "<html><body>"

              "<p>Device is restarting...Please wait...</p>"
              "<meta http-equiv='refresh' content='5;url=/'></body></html>");

  Serial.println("Reset requested - restarting device");
  delay(1000);    // Give time for response to be sent
  ESP.restart();  // This will call setup() again after reboot
}
void handleCSS() {
  String css = readFile("/styles.css");
  if (css == "") {
    css = "body { font-family: Arial, sans-serif; margin: 20px; }"
          "form { margin: 20px 0; }"
          "input { margin: 5px 0; }";
  }
  server.send(200, "text/css", css);
}
// Login handler
void handleLogin() {
  String user = server.arg("user");
  String pass = server.arg("pass");

  Serial.println(user + "=" + USERNAME);
  Serial.println(pass + "=" + PASSWORD);



  if (user == USERNAME && pass == PASSWORD) {
    loginErrorMessage = "";
    loginStatus = true;
    Serial.println("Login successful");
    server.sendHeader("Location", "/form1");

    server.send(302);
    return;

  } else {
    loginErrorMessage = "Login Failed. Try Again";
    server.sendHeader("Location", "/?login=failed");
    server.send(302);
    Serial.println("Login failed");
  }
}
void handleLoginPage() {

  loginStatus = false;



  String html = readFile("/login.html");

  deviceIPaddress = WiFi.localIP().toString();
  html.replace("{{ipAddress}}", deviceIPaddress);
  html.replace("{{loginErrorMessage}}", loginErrorMessage);


  server.send(200, "text/html", html);
}


// Serve index.html with dynamic values
void handleRoot() {

  if (!loginStatus) {
    server.sendHeader("Location", "/");
    server.send(302);
    return;
  }






  deviceIPaddress = WiFi.localIP().toString();

  if (!LittleFS.exists("/index.html")) {
    server.send(404, "text/plain", "404: Not Found");
    return;
  }

  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Internal Server Error: Could not open index.html");
    return;
  }

  String html = file.readString();
  file.close();

  // Replace placeholders with configuration values
  html.replace("{{serverURL}}", serverURL);
  html.replace("{{SerialNumber}}", serialNumber);
  html.replace("{{wifiSSID}}", wifiSSID);
  html.replace("{{wifiPassword}}", wifiPassword);
  html.replace("{{ipAddress}}", deviceIPaddress);
  html.replace("{{intervalHeartbeat}}", String(intervalHeartbeat));
  html.replace("{{gmtTimeZone}}", gmtTimeZone);
  html.replace("{{server_ip}}", server_ip);
  html.replace("{{server_port}}", server_port);

  html.replace("{{mqtt_server}}", mqtt_server);
  html.replace("{{mqtt_port}}", String(mqtt_port));
  html.replace("{{mqtt_clientId}}", mqtt_clientId);
  html.replace("{{password}}", PASSWORD);
  html.replace("{{firmWareVersion}}", firmWareVersion);


if(hasInternet )
  html.replace("{{hasInternet}}","<span style=color:green>Yes</span>");
else
  html.replace("{{hasInternet}}","<span style=red:green>No</span>");

  Serial.print("Version--------------------------------------------------------------------------");
  Serial.println(firmWareVersion);


  server.send(200, "text/html", html);
}


void handleLogoImage() {
  File file = LittleFS.open("/logo.png", "r");  // Open the file from SPIFFS
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }

  server.streamFile(file, "image/jpeg");  // Send the file over HTTP
  file.close();
}
void handleConfigJson() {
  if (!LittleFS.exists("/config.json")) {
    server.send(404, "application/json", "{\"error\": \"Config file not found\"}");
    return;
  }

  File file = LittleFS.open("/config.json", "r");
  String jsonContent = file.readString();
  file.close();

  server.send(200, "application/json", jsonContent);
}
// Save configuration handler
void handleSaveConfig() {
  if (server.hasArg("serverURL") && server.hasArg("wifiSSID") && server.hasArg("wifiPassword")) {
    serverURL = server.arg("serverURL");
    serialNumber = server.arg("SerialNumber");
    wifiSSID = server.arg("wifiSSID");
    wifiPassword = server.arg("wifiPassword");

    intervalHeartbeat = server.arg("intervalHeartbeat").toInt();

    if (intervalHeartbeat <= 10) intervalHeartbeat = 60;
    intervalHeartbeat = intervalHeartbeat;
    server_ip = server.arg("server_ip");
    server_port = server.arg("server_port");

    ipAddress = server.arg("ipAddress");
    mqtt_server = server.arg("mqtt_server");
    mqtt_port = server.arg("mqtt_port").toInt();
    mqtt_clientId = server.arg("mqtt_clientId");
    PASSWORD = server.arg("password");






    if (saveConfig()) {
      server.send(200, "text/html", "<html><body><h1>Configuration Saved! Restarting...</h1></body></html>");

      delay(2000);
      ESP.restart();  // full software restart


    } else {
      server.send(500, "text/html", "<html><body><h1>Failed to Save Configuration!</h1></body></html>");
    }
  } else {
    server.send(400, "text/plain", "Bad Request: Missing parameters");
  }
}

String readFile(String path) {
  File file = LittleFS.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading: " + path);
    return "no content in file : " + path;
  }
  String content = file.readString();
  file.close();
  return content;
}