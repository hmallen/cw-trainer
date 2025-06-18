const API_BASE = '';
const statusTable = document.getElementById('status-table');
const statsTable  = document.getElementById('stats-table');
const lastCmdEl   = document.getElementById('last-cmd');
const ipEl        = document.getElementById('device-ip');

// fetch device IP via window.location once loaded
ipEl.textContent = location.hostname;

// attach button handlers
[...document.querySelectorAll('button[data-cmd]')].forEach(btn => {
  btn.addEventListener('click', () => sendCmd(btn.dataset.cmd));
});

document.getElementById('reset-stats').addEventListener('click', () => {
  fetch(`${API_BASE}/api/stats`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ reset: true })
  }).then(refreshAll);
});

function renderTable(table, dataObj) {
  table.innerHTML = '';
  Object.entries(dataObj).forEach(([k, v]) => {
    const row = table.insertRow();
    row.insertCell().textContent = k;
    row.insertCell().textContent = v;
  });
}

async function getJSON(path) {
  const r = await fetch(`${API_BASE}${path}`);
  if (!r.ok) throw new Error(path + ' ' + r.status);
  return r.json();
}

async function refreshStatus() {
  try {
    const s = await getJSON('/api/status');
    renderTable(statusTable, s);
  } catch (e) { console.error(e); }
}

async function refreshStats() {
  try {
    const st = await getJSON('/api/stats');
    renderTable(statsTable, st);
  } catch (e) { console.error(e); }
}

async function refreshControl() {
  try {
    const c = await getJSON('/api/control');
    lastCmdEl.textContent = c.lastCmd || '-';
  } catch (e) { console.error(e); }
}

async function refreshAll() {
  await Promise.all([refreshStatus(), refreshStats(), refreshControl()]);
}

async function sendCmd(cmd) {
  await fetch(`${API_BASE}/api/control`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ cmd })
  });
  refreshControl();
}

refreshAll();
setInterval(refreshAll, 5000);