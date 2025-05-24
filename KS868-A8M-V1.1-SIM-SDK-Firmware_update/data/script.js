function toggleConnectionFields() {
  const connectionType = document.getElementById("wifi_or_ethernet").value;
  const wifiFields = document.getElementById("wifiFields");
  const wifiPasswordField = document.getElementById("wifiPasswordField");
  const wifiIpField = document.getElementById("wifiIpField");
  const wifigateway = document.getElementById("wifigateway");
  const wifiEthernet = document.getElementById("wifiEthernet");
  const ethernetFields = document.getElementById("ethernetFields");
  const ethGatewayField = document.getElementById("ethGatewayField");
  const ethSubnetField = document.getElementById("ethSubnetField");

  if (connectionType === "1") {
    // WiFi selected
    wifiFields.style.display = "";
    wifiPasswordField.style.display = "";
    wifiIpField.style.display = "";
    wifigateway.style.display = "";
    wifiEthernet.style.display = "";
    ethernetFields.style.display = "none";
    ethGatewayField.style.display = "none";
    ethSubnetField.style.display = "none";
  } else {
    // Ethernet selected
    wifiFields.style.display = "none";
    wifiPasswordField.style.display = "none";
    wifiIpField.style.display = "none";
    wifigateway.style.display = "none";
    wifiEthernet.style.display = "none";

    ethernetFields.style.display = "";
    ethGatewayField.style.display = "";
    ethSubnetField.style.display = "";
  }
}

// Initialize on page load //document ready
document.addEventListener("DOMContentLoaded", function () {
  toggleConnectionFields();

  let selectElement = document.getElementById("heartbeat");
  let options = generateTimeOptionsHeartbeat();

  options.forEach((opt) => {
    const optionElement = document.createElement("option");
    optionElement.value = opt.value;
    optionElement.textContent = opt.label;
    selectElement.appendChild(optionElement);
  });

  selectElement = document.getElementById("max_temperature_sensor_count");
  options = generateSensorCountOptions();

  options.forEach((opt) => {
    const optionElement = document.createElement("option");
    optionElement.value = opt.value;
    optionElement.textContent = opt.label;
    selectElement.appendChild(optionElement);
  });

  options = generateTimeOptions();

  selectElement = document.getElementById("max_doorcontact");

  options.forEach((opt) => {
    if (opt.value >= 60) {
      const optionElement = document.createElement("option");
      optionElement.value = opt.value / 60;
      optionElement.textContent = opt.label;
      selectElement.appendChild(optionElement);
    }
  });

  selectElement = document.getElementById("max_siren_play");

  options.forEach((opt) => {
    const optionElement = document.createElement("option");
    optionElement.value = opt.value;
    optionElement.textContent = opt.label;
    selectElement.appendChild(optionElement);
  });

  selectElement = document.getElementById("max_siren_pause");

  options.forEach((opt) => {
    if (opt.value >= 60) {
      const optionElement = document.createElement("option");
      optionElement.value = opt.value / 60;
      optionElement.textContent = opt.label;
      selectElement.appendChild(optionElement);
    }
  });
  loadConfigFileData();
});
function toggleInput(checkboxId, inputId) {
  document.getElementById(inputId).disabled =
    !document.getElementById(checkboxId).checked;
  if (!document.getElementById(checkboxId).checked) {
    document.getElementById(inputId).value = "";
  }
}

function toggleTempAlerts() {
  document.getElementById("temperature_alert_sms").disabled =
    !document.getElementById("temp_checkbox").checked;
  document.getElementById("temperature_alert_call").disabled =
    !document.getElementById("temp_checkbox").checked;
  document.getElementById("temperature_alert_whatsapp").disabled =
    !document.getElementById("temp_checkbox").checked;

  if (!document.getElementById("temp_checkbox").checked) {
    document.getElementById("temperature_alert_sms").checked = false;
    document.getElementById("temperature_alert_call").checked = false;
    document.getElementById("temperature_alert_whatsapp").checked = false;
  }
}
function togglehumidiyAlerts() {
  document.getElementById("humidity_alert_sms").disabled =
    !document.getElementById("humidity_checkbox").checked;
  document.getElementById("humidity_alert_call").disabled =
    !document.getElementById("humidity_checkbox").checked;
  document.getElementById("humidity_alert_whatsapp").disabled =
    !document.getElementById("humidity_checkbox").checked;

  if (!document.getElementById("humidity_checkbox").checked) {
    document.getElementById("humidity_alert_sms").checked = false;
    document.getElementById("humidity_alert_call").checked = false;
    document.getElementById("humidity_alert_whatsapp").checked = false;
  }
}
function toggleWaterAlerts() {
  document.getElementById("water_alert_sms").disabled =
    !document.getElementById("water_checkbox").checked;
  document.getElementById("water_alert_call").disabled =
    !document.getElementById("water_checkbox").checked;
  document.getElementById("water_alert_whatsapp").disabled =
    !document.getElementById("water_checkbox").checked;

  if (!document.getElementById("water_checkbox").checked) {
    document.getElementById("water_alert_sms").checked = false;
    document.getElementById("water_alert_call").checked = false;
    document.getElementById("water_alert_whatsapp").checked = false;
  }
}
function toggleFireAlerts() {
  document.getElementById("fire_alert_sms").disabled =
    !document.getElementById("fire_checkbox").checked;
  document.getElementById("fire_alert_call").disabled =
    !document.getElementById("fire_checkbox").checked;
  document.getElementById("fire_alert_whatsapp").disabled =
    !document.getElementById("fire_checkbox").checked;

  if (!document.getElementById("fire_checkbox").checked) {
    document.getElementById("fire_alert_sms").checked = false;
    document.getElementById("fire_alert_call").checked = false;
    document.getElementById("fire_alert_whatsapp").checked = false;
  }
}
function togglePowerAlerts() {
  document.getElementById("power_alert_sms").disabled =
    !document.getElementById("power_checkbox").checked;
  document.getElementById("power_alert_call").disabled =
    !document.getElementById("power_checkbox").checked;
  document.getElementById("power_alert_whatsapp").disabled =
    !document.getElementById("power_checkbox").checked;

  if (!document.getElementById("power_checkbox").checked) {
    document.getElementById("power_alert_sms").checked = false;
    document.getElementById("power_alert_call").checked = false;
    document.getElementById("power_alert_whatsapp").checked = false;
  }
}
function toggleDoorAlerts() {
  document.getElementById("door_alert_sms").disabled =
    !document.getElementById("door_checkbox").checked;
  document.getElementById("door_alert_call").disabled =
    !document.getElementById("door_checkbox").checked;
  document.getElementById("door_alert_whatsapp").disabled =
    !document.getElementById("door_checkbox").checked;

  if (!document.getElementById("door_checkbox").checked) {
    document.getElementById("door_alert_sms").checked = false;
    document.getElementById("door_alert_call").checked = false;
    document.getElementById("door_alert_whatsapp").checked = false;
  }
}
function generateSensorCountOptions() {
  const options = [];

  // Generate options for seconds (5s to 55s)
  for (let sec = 1; sec <= 9; sec += 1) {
    options.push({
      id: "max_temperature_sensor_count",
      value: sec,
      label: `${sec} Sensors`,
    });
  }

  return options;
}
function generateTimeOptionsHeartbeat() {
  const options = [];
  const increments = [5, 10, 15, 30]; // Time increments in seconds and minutes

  // Generate options for seconds (5s to 55s)
  for (let sec = 1; sec <= 10; sec += 1) {
    options.push({ id: "heartbeat", value: sec, label: `${sec} seconds` });
  }

  return options;
}
function generateTimeOptions() {
  const options = [];
  const increments = [5, 10, 15, 30]; // Time increments in seconds and minutes

  // Generate options for seconds (5s to 55s)
  for (let sec = 5; sec < 60; sec += 5) {
    options.push({ id: "heartbeat", value: sec, label: `${sec} seconds` });
  }

  // Generate options for minutes (1 min to 60 min)
  for (let min = 1; min <= 60; min += 1) {
    if (min == 0)
      options.push({
        id: "heartbeat",
        value: (min + 1) * 60,
        label: `${min + 1} minute${min + 1 > 1 ? "s" : ""}`,
      });
    else
      options.push({
        id: "heartbeat",
        value: min * 60,
        label: `${min} minute${min > 1 ? "s" : ""}`,
      });
  }

  return options;
}
function loadConfigFileData() {
  const content = document.getElementById("config_json").value;

  //   console.log(content); // Raw text content
  try {
    const jsonData = JSON.parse(content);
    // console.info(jsonData);

    // // Access individual properties
    // console.info("WiFi SSID:", jsonData.wifi_ssid);
    // console.info("Ethernet IP:", jsonData.eth_ip);
    // console.info("Server URL:", jsonData.server_url);

    document.getElementById("wifi_ssid").value =
      jsonData.wifi_ssid == "{wifi_ssid}" ? "" : jsonData.wifi_ssid;
    document.getElementById("wifi_password").value =
      jsonData.wifi_password == "{wifi_password}" ? "" : jsonData.wifi_password;
    document.getElementById("wifi_ip").value =
      jsonData.wifi_ip == "{wifi_ip}" ? "" : jsonData.wifi_ip;
    document.getElementById("wifi_or_ethernet").value =
      jsonData.wifi_or_ethernet == "{wifi_or_ethernet}"
        ? ""
        : jsonData.wifi_or_ethernet;

    document.getElementById("wifi_gateway").value =
      jsonData.wifi_gateway == "{wifi_gateway}" ? "" : jsonData.wifi_gateway;
    document.getElementById("wifi_subnet").value =
      jsonData.wifi_subnet == "{wifi_subnet}" ? "" : jsonData.wifi_subnet;

    document.getElementById("eth_ip").value =
      jsonData.eth_ip == "{eth_ip}" ? "" : jsonData.eth_ip;
    document.getElementById("eth_gateway").value =
      jsonData.eth_gateway == "{eth_gateway}" ? "" : jsonData.eth_gateway;
    document.getElementById("eth_subnet").value =
      jsonData.eth_subnet == "{eth_subnet}" ? "" : jsonData.eth_subnet;

    document.getElementById("device_serial_number").value =
      jsonData.device_serial_number == "{device_serial_number}"
        ? ""
        : jsonData.device_serial_number;

    document.getElementById("server_url").value =
      jsonData.server_url == "{server_url}" ? "" : jsonData.server_url;
    document.getElementById("heartbeat").value =
      jsonData.heartbeat == "{heartbeat}" ? "" : jsonData.heartbeat;
    document.getElementById("server_ip").value =
      jsonData.server_ip == "{server_ip}" ? "" : jsonData.server_ip;
    document.getElementById("server_port").value =
      jsonData.server_port == "{server_port}" ? "" : jsonData.server_port;

    document.getElementById("max_temperature_sensor_count").value =
      jsonData.max_temperature_sensor_count == "{max_temperature_sensor_count}"
        ? ""
        : jsonData.max_temperature_sensor_count;

    document.getElementById("min_temperature").value =
      jsonData.min_temperature == "{min_temperature}"
        ? ""
        : jsonData.min_temperature;
    document.getElementById("max_temperature").value =
      jsonData.max_temperature == "{max_temperature}"
        ? ""
        : jsonData.max_temperature;

    document.getElementById("max_humidity").value =
      jsonData.max_humidity == "{max_humidity}" ? "" : jsonData.max_humidity;

    document.getElementById("max_doorcontact").value =
      jsonData.max_doorcontact == "{max_doorcontact}"
        ? ""
        : jsonData.max_doorcontact;
    document.getElementById("max_siren_play").value =
      jsonData.max_siren_play == "{max_siren_play}"
        ? ""
        : jsonData.max_siren_play;
    document.getElementById("max_siren_pause").value =
      jsonData.max_siren_pause == "{max_siren_pause}"
        ? ""
        : jsonData.max_siren_pause;

    document.getElementById("temp_checkbox").checked = jsonData.temp_checkbox;
    document.getElementById("water_checkbox").checked = jsonData.water_checkbox;
    document.getElementById("fire_checkbox").checked = jsonData.fire_checkbox;
    document.getElementById("power_checkbox").checked = jsonData.power_checkbox;
    document.getElementById("door_checkbox").checked = jsonData.door_checkbox;
    document.getElementById("siren_checkbox").checked = jsonData.siren_checkbox;
    document.getElementById("humidity_checkbox").checked =
      jsonData.humidity_checkbox;

    document.getElementById("humidity_checkbox").checked =
      jsonData.humidity_checkbox;
    document.getElementById("humidity_checkbox").checked =
      jsonData.humidity_checkbox;
    document.getElementById("humidity_checkbox").checked =
      jsonData.humidity_checkbox;
    document.getElementById("humidity_checkbox").checked =
      jsonData.humidity_checkbox;

    for (let i = 0; i < 4; i++) {
      const element = document.getElementById("relaystatus" + i);
      const isOn = jsonData["relay" + i];
      element.innerHTML = isOn ? "ON" : "OFF";
      // element.style.backgroundColor = isOn ? 'green' : 'red';
      // element.style.color = 'white'; // ✅ Set font color to white
    }
    toggleInput("temp_checkbox", "max_temperature");
    toggleInput("temp_checkbox", "min_temperature");
    toggleInput("siren_checkbox", "max_siren_play");
    toggleInput("siren_checkbox", "max_siren_pause");
    toggleInput("door_checkbox", "max_doorcontact");
    toggleInput("humidity_checkbox", "max_humidity");

    toggleConnectionFields();
    toggleDoorAlerts();
    toggleFireAlerts();

    togglePowerAlerts();
    toggleTempAlerts();
    toggleWaterAlerts();
    togglehumidiyAlerts();

    console.log(jsonData.cloud);
  } catch (error) {
    console.error("Error parsing JSON:", error);
  }
}
function verifyform() {
  if (document.getElementById("temp_checkbox").checked) {
    let min_temperature = document.getElementById("min_temperature").value;
    let max_temperature = document.getElementById("max_temperature").value;

    min_temperature = parseFloat(min_temperature);
    if (isNaN(min_temperature)) {
      alert("Invalid Temperature Value");
      return false;
    }
  }
  alert("Ok");
  return true;

  return false; // Allow submission
}

function updateRelayStatus(relayid, divID) {
  document.getElementById(divID).innerHTML =
    document.getElementById(divID).innerHTML == "ON" ? "OFF" : "ON";
  fetch("http://" + ipAddress + "/changerelay_status?num=" + relayid) // Replace with your API
    .then((response) => response.json())
    .then((data) => {
      //   document.getElementById(divID).innerHTML =
      //     document.getElementById(divID).innerHTML == "ON" ? "OFF" : "ON";
    })
    .catch((error) => {
      console.error("Error fetching data:", error);
    });
}
let fetchdataloading = false;
function fetchData() {
  console.log("fetchdataloading", fetchdataloading);

  if (fetchdataloading) return false;

  fetchdataloading = true;
  fetch("http://" + ipAddress + "/getconfig") // Replace with your actual IP or hostname
    .then((response) => response.json())
    .then((data) => {
      //   console.log("Fetched data:", data);

      // If config_json is a <textarea>
      document.getElementById("config_json").value = JSON.stringify(
        data,
        null,
        2
      );

      // Call your config loader
      updateConfigFileData();

      fetchdataloading = false;
      console.log("fetchdataloading2", fetchdataloading);
    })
    .catch((error) => {
      console.error("Error fetching data:", error);
    });
}

function updateConfigFileData() {
  const content = document.getElementById("config_json").value;

  //   console.log(content); // Raw text content
  try {
    const jsonData = JSON.parse(content);

    for (let i = 0; i < 4; i++) {
      const element = document.getElementById("relaystatus" + i);
      const isOn = jsonData["relay" + i];
      element.innerHTML = isOn ? "ON" : "OFF";
      // element.style.backgroundColor = isOn ? 'green' : 'red';
      // element.style.color = 'white'; // ✅ Set font color to white
    }

    //update internet
    // INTERNET STATUS
    const internetEl = document.getElementById("internet");
    internetEl.innerText = jsonData.internet == "online" ? "Online" : "Offline";
    internetEl.style.color = jsonData.internet == "online" ? "green" : "red";

    // CLOUD STATUS
    const cloudEl = document.getElementById("cloud");
    cloudEl.innerText = jsonData.cloud == "online" ? "Online" : "Offline";
    cloudEl.style.color = jsonData.cloud == "online" ? "green" : "red";

    if (jsonData.cloudAccountActiveDaysRemaining <= 0) {
      alert("Your Account is Expired. Contact Service Provider.");

      document.getElementById("divAccountExpired").style.display = "block";
    } else if (jsonData.cloudAccountActiveDaysRemaining < 30) {
      alert(
        "Your Account will expire in " +
          jsonData.cloudAccountActiveDaysRemaining +
          " days and De-Activate Alarm Events automatically"
      );
    }
  } catch (error) {
    console.error("Error parsing JSON:", error);
  }
}

// Call fetchData every 5000 milliseconds (5 seconds)
setInterval(fetchData, 1000 * 15);
