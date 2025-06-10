function verifyform() {
  if (document.getElementById("mqtt_communication").textContent == "ON") {
    let mqtt_server = document.getElementById("mqtt_server").value;
    let mqtt_port = document.getElementById("mqtt_port").value;
    let mqtt_clientId = document.getElementById("mqtt_clientId").value;

    if (mqtt_server == "" || mqtt_port == "" || mqtt_clientId == "") {
      alert("MQQ Settings are Invalid");
      return false;
    }
  }
  if (document.getElementById("socket_communication").textContent == "ON") {
    if (
      document.getElementById("server_ip").value == "" ||
      document.getElementById("server_port").value == ""
    ) {
      alert("Socket  Settings are Invalid");
      return false;
    }
  }
  if (document.getElementById("http_communication").textContent == "ON") {
    if (document.getElementById("server_url").value == "") {
      alert("HTTP  Settings are Invalid");
      return false;
    }
  }
  return true;
}

// Initialize on page load //document ready
document.addEventListener("DOMContentLoaded", function () {
  try {
    loadConfigFileData();
  } catch (e) {}
});

function loadConfigFileData() {
  const content = document.getElementById("config_json").value;

  //   console.log(content); // Raw text content
  try {
    const jsonData = JSON.parse(content);

    updateHeaderConfigFileData();

    document.getElementById("mqtt_server").value = jsonData.mqtt_server;
    document.getElementById("mqtt_port").value = jsonData.mqtt_port;
    document.getElementById("mqtt_clientId").value = jsonData.mqtt_clientId;

    document.getElementById("server_ip").value = jsonData.server_ip;
    document.getElementById("server_port").value = jsonData.server_port;

    document.getElementById("server_url").value = jsonData.server_url;
  } catch (error) {
    console.error("Error parsing JSON:", error);
  }
}

function updateHeaderConfigFileData() {
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

function toggleMQTT() {
  const status = document.getElementById("mqtt_communication");
  const settings = document.getElementById("mqtt_settings");
  if (status.textContent === "OFF") {
    status.textContent = "ON";
    status.style.color = "green";
    settings.style.display = "block";
  } else {
    status.textContent = "OFF";
    status.style.color = "red";
    settings.style.display = "none";
  }
}

function toggleSocket() {
  const status = document.getElementById("socket_communication");
  const settings = document.getElementById("socket_settings");
  if (status.textContent === "OFF") {
    status.textContent = "ON";
    status.style.color = "green";
    settings.style.display = "block";
  } else {
    status.textContent = "OFF";
    status.style.color = "red";
    settings.style.display = "none";
  }
}

function toggleHTTP() {
  const status = document.getElementById("http_communication");
  const settings = document.getElementById("http_settings");
  if (status.textContent === "OFF") {
    status.textContent = "ON";
    status.style.color = "green";
    settings.style.display = "block";
  } else {
    status.textContent = "OFF";
    status.style.color = "red";
    settings.style.display = "none";
  }
}

// Example: Get all enabled config values
function getCommunicationConfigs() {
  return {
    mqtt:
      document.getElementById("mqtt_communication").textContent === "ON"
        ? {
            server: document.getElementById("mqtt_server").value,
            port: document.getElementById("mqtt_port").value,
            clientId: document.getElementById("mqtt_clientId").value,
          }
        : null,

    socket:
      document.getElementById("socket_communication").textContent === "ON"
        ? {
            ip: document.getElementById("server_ip").value,
            port: document.getElementById("server_port").value,
          }
        : null,

    http:
      document.getElementById("http_communication").textContent === "ON"
        ? {
            link: document.getElementById("server_url").value,
          }
        : null,
  };
}
