function updateTime() {
  const now = new Date();
  const hours = now.getHours().toString().padStart(2, "0");
  const minutes = now.getMinutes().toString().padStart(2, "0");
  const seconds = now.getSeconds().toString().padStart(2, "0");

  // Format the time as HH:MM:SS
  const timeString = `${hours}:${minutes}:${seconds}`;

  // Display the time in the HTML element
  document.getElementById("time").textContent = timeString;
}

// Update time immediately
updateTime();

// Update time every second (1000 milliseconds)
setInterval(updateTime, 1000);

// Initialize on page load //document ready
document.addEventListener("DOMContentLoaded", function () {
  updateHeaderConfigFileData();
  // const content = document.getElementById("config_json").value;

  // //   console.log(content); // Raw text content
  // try {
  //   const jsonData = JSON.parse(content);
  //   //update internet
  //   // INTERNET STATUS
  //   const internetEl = document.getElementById("internet");
  //   internetEl.innerText = jsonData.internet == "online" ? "Online" : "Offline";
  //   internetEl.style.color = jsonData.internet == "online" ? "green" : "red";

  //   // CLOUD STATUS
  //   const cloudEl = document.getElementById("cloud");
  //   cloudEl.innerText = jsonData.cloud == "online" ? "Online" : "Offline";
  //   cloudEl.style.color = jsonData.cloud == "online" ? "green" : "red";

  //   if (jsonData.cloudAccountActiveDaysRemaining <= 0) {
  //     alert("Your Account is Expired. Contact Service Provider.");

  //     document.getElementById("divAccountExpired").style.display = "block";
  //   } else if (jsonData.cloudAccountActiveDaysRemaining < 30) {
  //     alert(
  //       "Your Account will expire in " +
  //       jsonData.cloudAccountActiveDaysRemaining +
  //       " days and De-Activate Alarm Events automatically"
  //     );
  //   }
  // } catch (error) {
  //   console.error("Error parsing JSON:", error);
  // }
});
function updateRelaysData(jsonData) {
  for (let i = 0; i < 8; i++) {
    const element = document.getElementById("relaystatus" + i);
    const isOn = jsonData["relay" + i];
    element.innerHTML = isOn ? "ON" : "OFF";
    // element.style.backgroundColor = isOn ? 'green' : 'red';
    // element.style.color = 'white'; // ✅ Set font color to white
  }
}
function updateHeaderConfigFileData() {
  const content = document.getElementById("config_json").value;

  //   console.log(content); // Raw text content
  try {
    const jsonData = JSON.parse(content);

    try {
      updateRelaysData(jsonData);
    } catch (e) {}

    // for (let i = 0; i < 8; i++) {
    //   const element = document.getElementById("relaystatus" + i);
    //   const isOn = jsonData["relay" + i];
    //   element.innerHTML = isOn ? "ON" : "OFF";
    //   // element.style.backgroundColor = isOn ? 'green' : 'red';
    //   // element.style.color = 'white'; // ✅ Set font color to white
    // }

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

setInterval(updateHeaderConfigFileData, 1000 * 15);
