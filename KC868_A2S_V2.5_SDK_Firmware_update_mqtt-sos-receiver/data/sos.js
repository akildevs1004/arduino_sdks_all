// APIs:
// - GET  /api/sos/devices
// - GET  /api/sos/rf/last            -> {seq, code, bits, proto, pulse, atMs}
// - POST /api/sos/devices/upsert     -> save add/update to config.json
// - DELETE /api/sos/devices/delete?id=1
const API = {
  LIST: "/api/sos/devices",
  RF_LAST: "/api/sos/rf/last",
  UPSERT: "/api/sos/devices/upsert",
  DEL: "/api/sos/devices/delete",
};

const $ = (id) => document.getElementById(id);

function escapeHtml(s) {
  return String(s).replace(
    /[&<>"']/g,
    (c) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[
        c
      ])
  );
}
function logLine(line) {
  const el = $("log");
  const now = new Date().toLocaleString();
  if (el.textContent.trim() === "—") el.textContent = "";
  el.textContent += `[${now}] ${line}\n`;
  el.scrollTop = el.scrollHeight;
}
function toast(title, msg, ms = 2800) {
  $("toastTitle").textContent = title;
  $("toastMsg").textContent = msg;
  const t = $("toast");
  t.style.display = "block";
  clearTimeout(toast._t);
  toast._t = setTimeout(() => (t.style.display = "none"), ms);
}
function setConn(state, msg) {
  const dot = $("connDot"),
    txt = $("connText");
  txt.textContent = msg;
  dot.classList.remove("ok", "bad");
  if (state === "ok") dot.classList.add("ok");
  if (state === "bad") dot.classList.add("bad");
}

async function apiGet(url) {
  const r = await fetch(url, { cache: "no-store" });
  if (!r.ok) throw new Error(`GET ${url} failed (${r.status})`);
  return r.json();
}
async function apiPost(url, body) {
  const r = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
  if (!r.ok) {
    const t = await r.text().catch(() => "");
    throw new Error(`POST ${url} failed (${r.status}) ${t}`);
  }
  return r.json().catch(() => ({}));
}
async function apiDelete(url) {
  const r = await fetch(url, { method: "DELETE" });
  if (!r.ok) {
    const t = await r.text().catch(() => "");
    throw new Error(`DELETE failed (${r.status}) ${t}`);
  }
  return r.json().catch(() => ({}));
}

// ===== List/Table =====
let devices = [];
function statusChip(status) {
  const s = String(status || "").toUpperCase();
  if (s === "ON")
    return `<span class="chip on"><span class="miniDot"></span>ON</span>`;
  if (s === "OFF")
    return `<span class="chip off"><span class="miniDot"></span>OFF</span>`;
  return `<span class="chip"><span class="miniDot"></span>—</span>`;
}

function render() {
  $("count").textContent = String(devices.length);
  const tb = $("tbody");
  //   if (!devices.length) {
  //     tb.innerHTML = `<tr><td colspan="7" class="hint">0 SOS Devices registered.</td></tr>`;
  //     return;
  //   }
  tb.innerHTML = devices
    .map(
      (d) => `
        <tr>
          <td><b>${escapeHtml(d.id ?? "")}</b></td>
          <td>${escapeHtml(d.name ?? "")}</td>
          <td>${escapeHtml(d.roomId ?? "")}</td>
          <td><span class="mono">${escapeHtml(d.onCode ?? "")}</span></td>
          <td><span class="mono">${escapeHtml(d.offCode ?? "")}</span></td>
          <td>${statusChip(d.status)}</td>
          <td><button class="small danger" onclick="deleteDevice(${Number(
            d.id
          )})">Delete</button></td>
        </tr>
      `
    )
    .join("");
}

async function loadDevices() {
  try {
    const data = await apiGet(API.LIST);
    devices = Array.isArray(data) ? data : data.devices || [];
    setConn("ok", "Connected");
    render();
    logLine(`Loaded ${devices.length} Device(s)`);
  } catch (e) {
    setConn("bad", "Disconnected");
    devices = [];
    render();
    toast("Load failed", e.message, 4500);
    logLine(`ERROR: ${e.message}`);
  }
}

async function deleteDevice(id) {
  if (!Number.isFinite(id) || id <= 0) return;
  if (!confirm(`Delete device id ${id}?`)) return;
  try {
    await apiDelete(`${API.DEL}?id=${encodeURIComponent(String(id))}`);
    toast("Deleted", `Device ${id} removed.`);
    logLine(`Deleted device id=${id}`);
    await loadDevices();
  } catch (e) {
    toast("Delete failed", e.message, 4500);
    logLine(`ERROR: ${e.message}`);
  }
}
window.deleteDevice = deleteDevice;

// ===== Popup Auto-capture Flow =====
let pollTimer = null;
let baseSeq = 0;
let lastSeenSeq = 0;

let step = 1; // 1=ON wait, 2=OFF wait, 3=purpose input
let capturedOn = null; // {code,bits,proto}
let capturedOff = null; // {code,bits,proto}

let existingMatchId = 0; // if ON/OFF matches existing device, we update it

function setStepUI() {
  const dot = $("stepDot");
  dot.classList.remove("ok", "bad");

  if (step === 1) {
    toast("Step 1: Press SOS ON button on Device", "", 3000);

    $("stepTitle").textContent = "Step 1: Press SOS ON button on Device";
    // $("stepText").innerHTML =
    //"Waiting for a new RF code… once received, it will be recorded as <b>SOS ON</b>.";
    $("stepState").textContent = "Waiting";
    $("footerHint").textContent = "Step 1 is active: press SOS ON button.";
    $("purposeBlock").style.display = "none";
    $("btnSave").disabled = true;
  }
  if (step === 2) {
    toast("Step 2: Press SOS OFF button on Device", "", 3000);
    $("stepTitle2").textContent = "  Step 2: Press SOS OFF button on Device";
    $("stepText").innerHTML =
      "Waiting for the next new RF code… once received, it will be recorded as <b>SOS OFF</b>.";
    $("stepState").textContent = "Waiting";
    $("footerHint").textContent = "Step 2 is active: press SOS OFF button.";
    $("purposeBlock").style.display = "none";
    $("btnSave").disabled = true;
  }
  if (step === 3) {
    toast("Step 3: Now enter Device Name And Room Number ", "", 3000);
    $("stepTitle3").textContent = "Captured successfully";
    $("stepText").innerHTML =
      "<b>SOS ON</b> and <b>SOS OFF</b> are recorded. Now enter Device Name (Purpose) and Save.";
    $("stepState").textContent = "Ready";
    $("footerHint").textContent = "Enter Device Name (Purpose) and click Save.";
    $("purposeBlock").style.display = "";
    $("btnSave").disabled = false;
    dot.classList.add("ok");
  }
}

function resetPopup() {
  step = 1;
  capturedOn = null;
  capturedOff = null;
  existingMatchId = 0;

  $("stepTitle").textContent = "";
  $("stepTitle2").textContent = "";
  $("stepTitle3").textContent = "";

  $("onCode").textContent = "—";
  $("offCode").textContent = "—";

  $("stepDotSosOn").style.display = "none";

  $("stepDotSosoff").style.display = "none";

  $("onMeta").textContent = "bits: — | proto: —";
  $("offMeta").textContent = "bits: — | proto: —";
  $("name").value = "";
  $("roomId").value = "";

  setStepUI();
}

function findExistingByAnyCode(codeStr) {
  const c = String(codeStr || "").trim();
  if (!c) return 0;
  const d = devices.find(
    (x) => String(x.onCode || "") === c || String(x.offCode || "") === c
  );
  return d ? Number(d.id) : 0;
}

async function startPolling() {
  // read current seq as baseline, so we only capture "new presses" after popup opens
  const rf = await apiGet(API.RF_LAST);
  baseSeq = Number(rf.seq || 0);
  lastSeenSeq = baseSeq;
  logLine(`Popup baseline seq=${baseSeq}`);

  // poll frequently
  pollTimer = setInterval(async () => {
    try {
      const d = await apiGet(API.RF_LAST);
      const seq = Number(d.seq || 0);
      const code = String(d.code || "").trim();
      if (!code || seq <= lastSeenSeq) return;

      lastSeenSeq = seq;

      // Step 1: capture ON
      if (step === 1) {
        capturedOn = {
          code,
          bits: Number(d.bits || 0),
          proto: Number(d.proto || 0),
        };
        $("onCode").textContent = code;

        $("stepDotSosOn").style.display = "inline-block";

        $(
          "onMeta"
        ).textContent = `bits: ${capturedOn.bits} | proto: ${capturedOn.proto}`;
        $("stepDot").classList.add("ok");
        $("stepState").textContent = "SOS ON recorded";
        toast("Success", "SOS ON is recorded.", 2200);
        logLine(`Captured ON code=${code} seq=${seq}`);

        // if matches existing device, plan to update it
        existingMatchId = findExistingByAnyCode(code);
        if (existingMatchId) {
          const ex = devices.find((x) => Number(x.id) === existingMatchId);
          if (ex) {
            $("name").value = ex.name || "";
            $("roomId").value = ex.roomId || 0;
            logLine(
              `Matched existing device id=${existingMatchId} (will update)`
            );
          }
        }

        step = 2;
        setStepUI();
        return;
      }

      // Step 2: capture OFF (must be different from ON)
      if (step === 2) {
        if (capturedOn && code === capturedOn.code) {
          logLine(`Ignored OFF capture because code == ON code (${code})`);
          return;
        }
        capturedOff = {
          code,
          bits: Number(d.bits || 0),
          proto: Number(d.proto || 0),
        };
        $("offCode").textContent = code;
        $("stepDotSosoff").style.display = "inline-block";

        $(
          "offMeta"
        ).textContent = `bits: ${capturedOff.bits} | proto: ${capturedOff.proto}`;
        $("stepDot").classList.add("ok");
        $("stepState").textContent = "SOS OFF recorded";
        toast("Success", "SOS OFF is recorded.", 2200);
        logLine(`Captured OFF code=${code} seq=${seq}`);

        // if OFF matches existing and ON didn't, still update that device
        if (!existingMatchId) {
          existingMatchId = findExistingByAnyCode(code);
          if (existingMatchId) {
            const ex = devices.find((x) => Number(x.id) === existingMatchId);
            if (ex) {
              $("name").value = ex.name || "";
              $("roomId").value = ex.roomId || 0;
              logLine(
                `Matched existing device id=${existingMatchId} (will update)`
              );
            }
          }
        }

        step = 3; // now ask for purpose/name
        setStepUI();
      }
    } catch (e) {
      // avoid spamming toasts; log only
      logLine(`RF poll error: ${e.message}`);
    }
  }, 350);
}

function stopPolling() {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = null;
}

async function saveDevice() {
  if (!capturedOn || !capturedOff) return;

  const name = $("name").value.trim();
  if (!name) {
    toast("Missing", "Please enter Device Name (Purpose).", 3500);
    return;
  }

  const roomId = Number($("roomId").value) || 0;

  const payload = {
    id: existingMatchId || 0, // 0 => add new, >0 => update existing
    name,
    roomId,
    onCode: capturedOn.code,
    onBits: capturedOn.bits,
    onProto: capturedOn.proto,
    offCode: capturedOff.code,
    offBits: capturedOff.bits,
    offProto: capturedOff.proto,
  };

  try {
    $("btnSave").disabled = true;
    const res = await apiPost(API.UPSERT, payload);
    const savedId = Number((res.device && res.device.id) || payload.id || 0);

    toast("Saved", savedId ? `Saved device id=${savedId}` : "Saved", 2200);
    logLine(
      `Saved device (id=${savedId || "new"}) on=${payload.onCode} off=${
        payload.offCode
      }`
    );

    await loadDevices();
    closeModal();
  } catch (e) {
    toast("Save failed", e.message, 4500);
    logLine(`ERROR: ${e.message}`);
    $("btnSave").disabled = false;
  }
}

// ===== Modal open/close =====
async function openModal() {
  $("modalBack").style.display = "flex";
  resetPopup();
  await startPolling();
}

function closeModal() {
  stopPolling();
  $("modalBack").style.display = "none";
}

// ===== Wiring =====
$("btnReload").addEventListener("click", loadDevices);
$("btnOpen").addEventListener("click", openModal);
$("btnClose").addEventListener("click", closeModal);
$("modalBack").addEventListener("click", (e) => {
  if (e.target === $("modalBack")) closeModal();
});

$("btnClearLog").addEventListener("click", () => ($("log").textContent = "—"));
$("btnReset").addEventListener("click", resetPopup);
$("btnSave").addEventListener("click", saveDevice);

// Init
loadDevices();

let lastRfSeq = -1;
let refreshInFlight = false;

async function watchRfSeq() {
  try {
    const d = await fetch("/api/sos/rf/last", { cache: "no-store" }).then((r) =>
      r.json()
    );
    const seq = Number(d.seq || 0);

    if (lastRfSeq === -1) lastRfSeq = seq;

    if (seq !== lastRfSeq && !refreshInFlight) {
      lastRfSeq = seq;
      refreshInFlight = true;
      await loadDevices();
      refreshInFlight = false;
    }
  } catch (e) {}
}

setInterval(watchRfSeq, 500);
