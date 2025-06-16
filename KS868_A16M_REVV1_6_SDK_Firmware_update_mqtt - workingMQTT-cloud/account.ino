 

void getDeviceAccountDetails() {
  // String company = "No COMPANY";
  // String accountExpiry = "Not Set";
  // cloudAccountActiveDaysRemaining = 0;

  // StaticJsonDocument<512> accountDoc;
  // const int retryCount = 1;

 
  //   String url = serverURL + "/get_device_company_info_arduino?serial_number=" + device_serial_number;
  //   HTTPClient http;
  //   http.begin(url);
  //   http.setTimeout(5000);

  //   int httpCode = http.GET();
  //   if (httpCode == HTTP_CODE_OK) {
  //     DeserializationError error = deserializeJson(accountDoc, http.getString());
  //     if (!error && accountDoc.containsKey("company")) {
  //       JsonObject companyObj = accountDoc["company"];
  //       company = companyObj["name"] | "No COMPANY";
  //       accountExpiry = companyObj["expiry"] | "Not Set";
  //       cloudAccountActiveDaysRemaining = companyObj["remaining_days"] | 0;
  //     }
  //   }

  //   http.end();
  //   if (company == "No COMPANY") delay(1000); // Retry delay if failed
 

  // updateJsonConfig("config.json", "cloud_company_name", company);
  // updateJsonConfig("config.json", "cloud_account_expire", accountExpiry);
  // updateJsonConfig("config.json", "cloudAccountActiveDaysRemaining", String(cloudAccountActiveDaysRemaining));
 
}
