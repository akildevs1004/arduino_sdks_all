
void routes() {
  server.on("/", HTTP_GET, handleLoginPage);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", HTTP_GET, handleLogout);
  server.on("/form1", HTTP_GET, handleForm1);
  // server.on("/form2", HTTP_GET, handleForm2);
  server.on("/submit-form1", HTTP_POST, handleForm1Submit);
  // server.on("/submit-form2", HTTP_POST, handleForm2Submit);
  server.on("/styles.css", HTTP_GET, handleCSS);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/logo", HTTP_GET, handleLogoImage);

  // server.on("/relayslist", HTTP_GET, handleRelaysList);
  server.on("/relay", HTTP_GET, handleRelayControl);
  // server.on("/changerelay_status", HTTP_GET, handleRelayChangeStatus);



  server.on("/changerelay_status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String relay = server.arg("num");  // e.g., "1"
    int relayNum = relay.toInt();
    updateRelayStatus(relayNum);

    server.send(200, "application/text", "Updated");  // your JSON config
  });



  // server.on("/getconfig", HTTP_GET, handleGetConfig);

  server.on("/getconfig", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");    // Allow all origins
    server.send(200, "application/json", handleGetConfig());  // your JSON config
  });






  server.on("/restart", HTTP_GET, handleRestartDevice);

  // server.on("/updatefirmware", HTTP_GET, handleUpdateFirmware);


  // server.on("/updatefirmwaredatafiles", HTTP_GET, handleUpdatePage);
  // server.on("/updatefirmwaredatafilessubmit", HTTP_POST, handleFileUpload);
  //server.onNotFound(handleNotFound);
}



// Check if user is authenticated
bool isAuthenticated() {


  return loginStatus;


  if (server.hasHeader("Cookie")) {
    String cookie = server.header("Cookie");
    Serial.println("Raw Cookie: " + cookie);  // Debug output

    int tokenIndex = cookie.indexOf("ESPSESSIONID=");
    if (tokenIndex != -1) {
      int endIndex = cookie.indexOf(";", tokenIndex);
      String token = endIndex == -1 ? cookie.substring(tokenIndex + 13) : cookie.substring(tokenIndex + 13, endIndex);

      if (token == sessionToken) {
        Serial.println("Valid session token found");
        return true;
      }
    }
  }
  Serial.println("No valid session token found");
  return false;
}
String handleGetConfig() {
  String savedData = readConfig("config.json");

  return savedData;
}
// Login page
void handleLoginPage() {

  loginStatus = false;

  if (isAuthenticated()) {
    server.sendHeader("Location", "/form1");
    server.send(302);
    return;
  }

  String html = readFile("/login.html");
  // if (html == "") {
  //   html = "<html><body><h2>Login111111111111111</h2><form action='/login' method='POST'>"
  //          "User:<input type='text' name='user'><br>"
  //          "Password:<input type='password' name='pass'><br>"
  //          "<input type='submit' value='Login'></form></body></html>";
  // }
  html.replace("{firmWareVersion}", firmWareVersion);
  html.replace("{ipAddress}", DeviceIPNumber);
  html.replace("{loginErrorMessage}", loginErrorMessage);


  server.send(200, "text/html", html);
}

// Login handler
void handleLogin() {
  String user = server.arg("user");
  String pass = server.arg("pass");

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

// Logout handler
void handleLogout() {
  sessionToken = "";
  String header = "HTTP/1.1 301 OK\r\nSet-Cookie: ESPSESSIONID=0\r\nExpires=Thu, 01 Jan 1970 00:00:00 GMT\r\nLocation: /\r\n\r\n";
  server.sendContent(header);
}

// Form 1
void handleForm1() {


  if (!isAuthenticated()) {
    server.sendHeader("Location", "/");
    server.send(302);
    return;
  }
  String header = readFile("/header.html");
  String form = readFile("/form1.html");

  String html;
  html.reserve(header.length() + form.length());
  html = header + form;

  // Read saved data
  String savedData = readConfig("config.json");


  String field1Value = "";

  if (savedData != "") {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, savedData);
    html.replace("{config_json}", savedData);


    html = replaceHeaderContent(html);


  } else {
    Serial.println("Form1 Content is empty");
  }

  Serial.println("Form1 Available");

  server.send(200, "text/html", html);
}

// Handle Form 1 submission
void handleForm1Submit() {
  if (!isAuthenticated()) {
    server.sendHeader("Location", "/");
    server.send(302);
    return;
  }

  DynamicJsonDocument doc(256);
  doc["wifi_ssid"] = server.arg("wifi_ssid");
  doc["wifi_password"] = server.arg("wifi_password");
  doc["wifi_ip"] = server.arg("wifi_ip");
  doc["wifi_or_ethernet"] = server.arg("wifi_or_ethernet");

  doc["wifi_gateway"] = server.arg("wifi_gateway");
  doc["wifi_subnet"] = server.arg("wifi_subnet");

  doc["eth_ip"] = server.arg("eth_ip");
  doc["eth_gateway"] = server.arg("eth_gateway");
  doc["eth_subnet"] = server.arg("eth_subnet");

  doc["device_serial_number"] = device_serial_number;  //config["device_serial_number"].as<String>() ;//server.arg("device_serial_number");


  doc["server_url"] = server.arg("server_url");
  doc["heartbeat"] = server.arg("heartbeat");
  doc["server_ip"] = server.arg("server_ip");
  doc["server_port"] = server.arg("server_port");

  doc["min_temperature"] = server.arg("min_temperature");
  doc["max_temperature"] = server.arg("max_temperature");


  doc["max_humidity"] = server.arg("max_humidity");


  doc["max_doorcontact"] = server.arg("max_doorcontact");
  doc["max_siren_play"] = server.arg("max_siren_play");
  doc["max_siren_pause"] = server.arg("max_siren_pause");




  doc["temp_checkbox"] = server.hasArg("temp_checkbox");
  doc["temperature_alert_sms"] = server.hasArg("temperature_alert_sms");
  doc["temperature_alert_call"] = server.hasArg("temperature_alert_call");
  doc["temperature_alert_whatsapp"] = server.hasArg("temperature_alert_whatsapp");

  doc["humidity_checkbox"] = server.hasArg("humidity_checkbox");
  doc["humidity_alert_sms"] = server.hasArg("humidity_alert_sms");
  doc["humidity_alert_call"] = server.hasArg("humidity_alert_call");
  doc["humidity_alert_whatsapp"] = server.hasArg("humidity_alert_whatsapp");

  doc["water_checkbox"] = server.hasArg("water_checkbox");
  doc["water_alert_sms"] = server.hasArg("water_alert_sms");
  doc["water_alert_call"] = server.hasArg("water_alert_call");
  doc["water_alert_whatsapp"] = server.hasArg("water_alert_whatsapp");

  doc["fire_checkbox"] = server.hasArg("fire_checkbox");
  doc["fire_alert_sms"] = server.hasArg("fire_alert_sms");
  doc["fire_alert_call"] = server.hasArg("fire_alert_call");
  doc["fire_alert_whatsapp"] = server.hasArg("fire_alert_whatsapp");

  doc["power_checkbox"] = server.hasArg("power_checkbox");
  doc["power_alert_sms"] = server.hasArg("power_alert_sms");
  doc["power_alert_call"] = server.hasArg("power_alert_call");
  doc["power_alert_whatsapp"] = server.hasArg("power_alert_whatsapp");

  doc["door_checkbox"] = server.hasArg("door_checkbox");
  doc["door_alert_sms"] = server.hasArg("door_alert_sms");
  doc["door_alert_call"] = server.hasArg("door_alert_call");
  doc["door_alert_whatsapp"] = server.hasArg("door_alert_whatsapp");

  Serial.println(server.hasArg("door_checkbox"));
  Serial.println(server.arg("door_checkbox"));



  String output;
  serializeJson(doc, output);
  saveConfig("config.json", output);

  // server.send(200, "text/plain", "Form 1 data saved successfully and Device is restarted....");

  // handleRestartDevice();
  Serial.println("Saved Config Data");
  Serial.println(output);
  GlobalWebsiteResponseMessage = "Data saved successfully";
  server.sendHeader("Location", "/form1");
  server.send(302);
  Serial.println("Data saved successfully");
  readConfig("config.json");


  return;
}

void handleUpdateFirmware() {
}

void handleRestartDevice() {
  // if (!isAuthenticated()) {
  //   server.sendHeader("Location", "/");
  //   server.send(302);
  //   return;
  // }

  server.send(200, "text/html",
              "<html><body>"

              "<p>Device is restarting...Please wait...</p>"
              "<meta http-equiv='refresh' content='5;url=/'></body></html>");

  Serial.println("Reset requested - restarting device");
  delay(1000);    // Give time for response to be sent
  ESP.restart();  // This will call setup() again after reboot
}

// Handle Form 2 submission
// void handleForm2Submit() {
//   if (!isAuthenticated()) {
//     server.sendHeader("Location", "/");
//     server.send(302);
//     return;
//   }

//   DynamicJsonDocument doc(256);
//   doc["fieldA"] = server.arg("fieldA");
//   doc["fieldB"] = server.arg("fieldB");

//   String output;
//   serializeJson(doc, output);
//   saveConfig("form2_config.json", output);

//   server.send(200, "text/plain", "Form 2 data saved successfully");
// }

// Serve CSS
void handleCSS() {
  String css = readFile("/styles.css");
  if (css == "") {
    css = "body { font-family: Arial, sans-serif; margin: 20px; }"
          "form { margin: 20px 0; }"
          "input { margin: 5px 0; }";
  }
  server.send(200, "text/css", css);
}


void handleStatus() {
  if (!isAuthenticated()) {
    server.sendHeader("Location", "/");
    server.send(302);
    return;
  }

  String html = "<html><head><title>Network Status</title>";
  html += "<style>body {font-family: Arial; margin: 20px;}";
  html += ".status {padding: 10px; margin: 10px 0; border-radius: 5px;}";
  html += ".online {background-color: #d4edda; color: #155724;}";
  html += ".offline {background-color: #f8d7da; color: #721c24;}";
  html += "</style></head><body>";
  html += "<h1>Network Status</h1>";

  // WiFi Status
  html += "<h2>WiFi</h2>";
  html += "<div class='status ";
  html += (WiFi.status() == WL_CONNECTED ? "online" : "offline");
  html += "'>";
  html += "<strong>Status:</strong> " + getWiFiStatus() + "<br>";
  html += "<strong>SSID:</strong> " + String(WiFi.SSID()) + "<br>";
  html += "<strong>Signal:</strong> " + String(WiFi.RSSI()) + " dBm<br>";
  html += "</div>";

  // Rest of your HTML...
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