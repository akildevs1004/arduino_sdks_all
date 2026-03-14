// APIs:
// - GET  /api/sos/devices
// - GET  /api/sos/rf/last
// - POST /api/sos/devices/upsert
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
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[
        c
      ],
  );
}

function logLine(line) {
  const el = $("log");
  const now = new Date().toLocaleString();
  if (!el) return;
  if (el.textContent.trim() === "—") el.textContent = "";
  el.textContent += `[${now}] ${line}\n`;
  el.scrollTop = el.scrollHeight;
}

function toast(title, msg, ms = 2800) {
  $("toastTitle").textContent = title;
  $("toastMsg").textContent = msg || "";
  const t = $("toast");
  t.style.display = "block";
  clearTimeout(toast._t);
  toast._t = setTimeout(() => (t.style.display = "none"), ms);
}

function setConn(state, msg) {
  const dot = $("connDot");
  const txt = $("connText");
  if (!dot || !txt) return;

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

  if (s === "ON") {
    return `<span class="chip on"><span class="miniDot"></span>ON</span>`;
  }
  if (s === "OFF") {
    return `<span class="chip off"><span class="miniDot"></span>OFF</span>`;
  }
  if (s === "THREAD_ON") {
    return `<span class="chip thread"><span class="miniDot"></span>THREAD ON</span>`;
  }

  return `<span class="chip"><span class="miniDot"></span>—</span>`;
}

function render() {
  $("count").textContent = String(devices.length);
  const tb = $("tbody");

  if (!devices.length) {
    tb.innerHTML = `<tr><td colspan="8" class="hint">No SOS devices registered.</td></tr>`;
    return;
  }

  tb.innerHTML = devices
    .map(
      (d) => `
        <tr>
          <td><b>${escapeHtml(d.id ?? "")}</b></td>
          <td>${escapeHtml(d.name ?? "")}</td>
          <td>${escapeHtml(d.roomId ?? "")}</td>
          <td><span class="mono">${escapeHtml(d.onCode ?? "")}</span></td>
          <td><span class="mono">${escapeHtml(d.offCode ?? "")}</span></td>
          <td><span class="mono">${escapeHtml(d.threadOnCode ?? "")}</span></td>
          <td>${statusChip(d.status)}</td>
          <td>
            <button class="small danger" onclick="deleteDevice(${Number(d.id)})">Delete</button>
          </td>
        </tr>
      `,
    )
    .join("");
}

async function loadDevices() {
  try {
    const data = await apiGet(API.LIST);
    devices = Array.isArray(data) ? data : data.devices || [];
    setConn("ok", "Connected");
    render();
    logLine(`Loaded ${devices.length} device(s)`);
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
let lastSeenSeq = 0;

// 1 = wait SOS ON, 2 = wait SOS OFF, 3 = wait THREAD ON, 4 = details/save
let step = 1;

let capturedOn = null;
let capturedOff = null;
let capturedThreadOn = null;
let existingMatchId = 0;

function clearStepDot() {
  $("stepDot").classList.remove("ok", "bad");
}

function setTinyDot(elId, ok) {
  const el = $(elId);
  if (!el) return;
  el.style.display = ok ? "inline-block" : "none";
  el.classList.toggle("ok", !!ok);
}

function normalizeCode(v) {
  return String(v || "").trim();
}

function findExistingByAnyCode(codeStr) {
  const c = normalizeCode(codeStr);
  if (!c) return 0;

  const d = devices.find(
    (x) =>
      normalizeCode(x.onCode) === c ||
      normalizeCode(x.offCode) === c ||
      normalizeCode(x.threadOnCode) === c,
  );

  return d ? Number(d.id) : 0;
}

function fillFromExistingIfMatched(id) {
  if (!id) return;

  const ex = devices.find((x) => Number(x.id) === id);
  if (!ex) return;

  if (!$("name").value.trim()) $("name").value = ex.name || "";
  if (!$("roomId").value.trim()) $("roomId").value = ex.roomId || 0;

  logLine(`Matched existing device id=${id} (will update)`);
}

function validateSaveEnabled() {
  const name = $("name").value.trim();
  const canSave = !!capturedOn && !!capturedOff && !!capturedThreadOn && !!name;
  $("btnSave").disabled = !canSave;
}

function setStepUI() {
  clearStepDot();

  // Always keep name / room visible
  $("purposeBlock").style.display = "block";

  if (step === 1) {
    $("stepTitle").textContent = "Step 1: Press SOS ON button on Device";
    $("stepText").innerHTML =
      "Waiting for a new RF code. Once received, it will be recorded as <b>SOS ON</b>.";
    $("stepState").textContent = "Waiting";
    $("footerHint").textContent = "Step 1 is active: press SOS ON button.";
    validateSaveEnabled();
    return;
  }

  if (step === 2) {
    $("stepTitle").textContent = "Step 2: Press SOS OFF button on Device";
    $("stepText").innerHTML = "SOS ON captured. Waiting for <b>SOS OFF</b>.";
    $("stepState").textContent = "Waiting";
    $("footerHint").textContent = "Step 2 is active: press SOS OFF button.";
    validateSaveEnabled();
    return;
  }

  if (step === 3) {
    $("stepTitle").textContent = "Step 3: Press SOS ON Thread button on Device";
    $("stepText").innerHTML =
      "SOS ON and SOS OFF captured. Waiting for <b>SOS ON Thread</b>. Functionally this is also treated as <b>ON</b>.";
    $("stepState").textContent = "Waiting";
    $("footerHint").textContent =
      "Step 3 is active: press SOS ON Thread button.";
    validateSaveEnabled();
    return;
  }

  if (step === 4) {
    $("stepTitle").textContent = "All required codes captured";
    $("stepText").innerHTML =
      "Enter <b>Device Name</b> and optional <b>Room ID</b>, then click <b>Save Device</b>.";
    $("stepState").textContent = "Ready";
    $("footerHint").textContent = "Enter details and click Save Device.";
    $("stepDot").classList.add("ok");
    validateSaveEnabled();
  }
}

function resetPopup() {
  step = 1;
  capturedOn = null;
  capturedOff = null;
  capturedThreadOn = null;
  existingMatchId = 0;

  $("onCode").textContent = "—";
  $("offCode").textContent = "—";
  $("threadOnCode").textContent = "—";

  $("onMeta").textContent = "bits: — | proto: —";
  $("offMeta").textContent = "bits: — | proto: —";
  $("threadOnMeta").textContent = "bits: — | proto: —";

  $("name").value = "";
  $("roomId").value = "";

  setTinyDot("stepDotSosOn", false);
  setTinyDot("stepDotSosOff", false);
  setTinyDot("stepDotThreadOn", false);

  setStepUI();
}

function codeAlreadyCaptured(code) {
  const c = normalizeCode(code);
  return (
    (capturedOn && normalizeCode(capturedOn.code) === c) ||
    (capturedOff && normalizeCode(capturedOff.code) === c) ||
    (capturedThreadOn && normalizeCode(capturedThreadOn.code) === c)
  );
}

async function startPolling() {
  const rf = await apiGet(API.RF_LAST);
  lastSeenSeq = Number(rf.seq || 0);
  logLine(`Popup baseline seq=${lastSeenSeq}`);

  stopPolling();

  pollTimer = setInterval(async () => {
    try {
      const d = await apiGet(API.RF_LAST);
      const seq = Number(d.seq || 0);
      const code = normalizeCode(d.code);

      if (!code || seq <= lastSeenSeq) return;
      lastSeenSeq = seq;

      if (step === 1) {
        capturedOn = {
          code,
          bits: Number(d.bits || 0),
          proto: Number(d.proto || 0),
        };

        $("onCode").textContent = code;
        $("onMeta").textContent =
          `bits: ${capturedOn.bits} | proto: ${capturedOn.proto}`;
        setTinyDot("stepDotSosOn", true);

        $("stepDot").classList.add("ok");
        $("stepState").textContent = "SOS ON recorded";
        toast("Captured", "SOS ON recorded.", 2000);
        logLine(`Captured ON code=${code} seq=${seq}`);

        existingMatchId = findExistingByAnyCode(code);
        fillFromExistingIfMatched(existingMatchId);

        step = 2;
        setStepUI();
        return;
      }

      if (step === 2) {
        if (capturedOn && normalizeCode(capturedOn.code) === code) {
          logLine(`Ignored OFF capture because code == ON (${code})`);
          return;
        }

        capturedOff = {
          code,
          bits: Number(d.bits || 0),
          proto: Number(d.proto || 0),
        };

        $("offCode").textContent = code;
        $("offMeta").textContent =
          `bits: ${capturedOff.bits} | proto: ${capturedOff.proto}`;
        setTinyDot("stepDotSosOff", true);

        $("stepDot").classList.add("ok");
        $("stepState").textContent = "SOS OFF recorded";
        toast("Captured", "SOS OFF recorded.", 2000);
        logLine(`Captured OFF code=${code} seq=${seq}`);

        if (!existingMatchId) {
          existingMatchId = findExistingByAnyCode(code);
          fillFromExistingIfMatched(existingMatchId);
        }

        step = 3;
        setStepUI();
        return;
      }

      if (step === 3) {
        if (codeAlreadyCaptured(code)) {
          logLine(
            `Ignored THREAD ON capture because code already used (${code})`,
          );
          return;
        }

        capturedThreadOn = {
          code,
          bits: Number(d.bits || 0),
          proto: Number(d.proto || 0),
        };

        $("threadOnCode").textContent = code;
        $("threadOnMeta").textContent =
          `bits: ${capturedThreadOn.bits} | proto: ${capturedThreadOn.proto}`;
        setTinyDot("stepDotThreadOn", true);

        $("stepDot").classList.add("ok");
        $("stepState").textContent = "SOS ON Thread recorded";
        toast("Captured", "SOS ON Thread recorded.", 2000);
        logLine(`Captured THREAD ON code=${code} seq=${seq}`);

        if (!existingMatchId) {
          existingMatchId = findExistingByAnyCode(code);
          fillFromExistingIfMatched(existingMatchId);
        }

        step = 4;
        setStepUI();
      }
    } catch (e) {
      logLine(`RF poll error: ${e.message}`);
    }
  }, 350);
}

function stopPolling() {
  if (pollTimer) clearInterval(pollTimer);
  pollTimer = null;
}

async function saveDevice() {
  if (!capturedOn) {
    toast("Missing", "Please capture SOS ON first.", 3500);
    return;
  }
  if (!capturedOff) {
    toast("Missing", "Please capture SOS OFF second.", 3500);
    return;
  }
  if (!capturedThreadOn) {
    toast("Missing", "Please capture SOS ON Thread third.", 3500);
    return;
  }

  const name = $("name").value.trim();
  if (!name) {
    toast("Missing", "Please enter Device Name.", 3500);
    return;
  }

  const roomId = Number($("roomId").value) || 0;

  const payload = {
    id: existingMatchId || 0,
    name,
    roomId,

    onCode: capturedOn.code,
    onBits: capturedOn.bits,
    onProto: capturedOn.proto,

    offCode: capturedOff.code,
    offBits: capturedOff.bits,
    offProto: capturedOff.proto,

    threadOnCode: capturedThreadOn.code,
    threadOnBits: capturedThreadOn.bits,
    threadOnProto: capturedThreadOn.proto,
  };

  try {
    $("btnSave").disabled = true;

    const res = await apiPost(API.UPSERT, payload);
    const savedId = Number((res.device && res.device.id) || payload.id || 0);

    toast("Saved", savedId ? `Saved device id=${savedId}` : "Saved", 2200);
    logLine(
      `Saved device (id=${savedId || "new"}) on=${payload.onCode} off=${payload.offCode} thread=${payload.threadOnCode}`,
    );

    await loadDevices();
    closeModal();
  } catch (e) {
    toast("Save failed", e.message, 4500);
    logLine(`ERROR: ${e.message}`);
    validateSaveEnabled();
  }
}

// ===== Modal =====
async function openModal() {
  $("modalBack").style.display = "flex";
  resetPopup();
  try {
    await startPolling();
  } catch (e) {
    toast("Open failed", e.message, 4500);
    logLine(`ERROR: ${e.message}`);
  }
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
$("name").addEventListener("input", validateSaveEnabled);
$("roomId").addEventListener("input", validateSaveEnabled);

// ===== Init =====
loadDevices();

// optional auto refresh when RF seq changes
let lastRfSeq = -1;
let refreshInFlight = false;

async function watchRfSeq() {
  try {
    const d = await fetch(API.RF_LAST, { cache: "no-store" }).then((r) =>
      r.json(),
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
