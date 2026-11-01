// ============================================================
// Unified Engineering Workbench — app.js
// All computation runs in the same-origin C++ HTTP API.
// ============================================================

const API = '';
const serverAddress = document.getElementById('server-address');
if (serverAddress) serverAddress.textContent = window.location.host;

// ── Tab Navigation ────────────────────────────────────────────────────────────
function showTab(tabId) {
  document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.tab-link').forEach(b => b.classList.remove('active'));
  document.getElementById(tabId).classList.add('active');
  document.querySelector(`[data-tab="${tabId}"]`).classList.add('active');
  if (tabId === 'geo-tab' && renderer) {
    setTimeout(() => renderer.setSize(
      document.getElementById('three-canvas').clientWidth,
      document.getElementById('three-canvas').clientHeight
    ), 100);
  }
}

document.querySelectorAll('.tab-link').forEach(btn => {
  btn.addEventListener('click', () => showTab(btn.dataset.tab));
});

// ── Utility ───────────────────────────────────────────────────────────────────
function showError(containerId, msg) {
  const el = document.getElementById(containerId);
  if (el) { el.textContent = msg; el.style.display = 'block'; }
}
function hideError(containerId) {
  const el = document.getElementById(containerId);
  if (el) el.style.display = 'none';
}
function setLoading(btnId, loading, label = '') {
  const btn = document.getElementById(btnId);
  if (!btn) return;
  btn.disabled = loading;
  btn.textContent = loading ? '⏳ Working…' : (label || btn.dataset.label || btn.textContent);
}
function downloadCSV(filename, content) {
  const a = document.createElement('a');
  a.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(content);
  a.download = filename;
  a.click();
}
async function apiPost(path, body) {
  const res = await fetch(API + path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}: ${res.statusText}`);
  return res.json();
}
async function apiGet(path) {
  const res = await fetch(API + path);
  if (!res.ok) throw new Error(`HTTP ${res.status}: ${res.statusText}`);
  return res.json();
}

// ── TAB 1: GEOMETRY (Task B) ──────────────────────────────────────────────────
let scene, camera, renderer, controls;
let stlMesh = null, csvPoints = null;
let csvNodes = []; // for click inspection

function initThreeJS() {
  const container = document.getElementById('three-canvas');
  if (!container || renderer) return;

  scene = new THREE.Scene();
  scene.background = new THREE.Color(0x1a1a2e);

  camera = new THREE.PerspectiveCamera(45, container.clientWidth / container.clientHeight, 0.001, 1000);
  camera.position.set(2, 2, 4);

  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(container.clientWidth, container.clientHeight);
  container.appendChild(renderer.domElement);

  // Lights
  const ambient = new THREE.AmbientLight(0xffffff, 0.4);
  scene.add(ambient);
  const dir1 = new THREE.DirectionalLight(0xffffff, 0.8);
  dir1.position.set(5, 10, 7);
  scene.add(dir1);
  const dir2 = new THREE.DirectionalLight(0x4488ff, 0.3);
  dir2.position.set(-5, -3, -5);
  scene.add(dir2);

  // Grid
  const grid = new THREE.GridHelper(10, 20, 0x333355, 0x222244);
  scene.add(grid);

  // OrbitControls
  if (typeof THREE.OrbitControls !== 'undefined') {
    controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
  }

  // Raycaster for node picking
  const raycaster = new THREE.Raycaster();
  raycaster.params.Points.threshold = 0.05;
  const mouse = new THREE.Vector2();
  renderer.domElement.addEventListener('click', (e) => {
    if (!csvPoints || csvNodes.length === 0) return;
    const rect = renderer.domElement.getBoundingClientRect();
    mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
    raycaster.setFromCamera(mouse, camera);
    const hits = raycaster.intersectObject(csvPoints);
    if (hits.length > 0) {
      const idx = hits[0].index;
      const n = csvNodes[idx];
      document.getElementById('node-inspector').innerHTML =
        `<strong>Node ${n.id}</strong><br>` +
        `x=${n.x.toFixed(4)} m, y=${n.y.toFixed(4)} m, z=${n.z.toFixed(4)} m<br>` +
        `<span style="color:#e94560">T = ${n.temp.toFixed(2)} °C</span>`;
    }
  });

  // Resize handler
  window.addEventListener('resize', () => {
    if (!renderer) return;
    camera.aspect = container.clientWidth / container.clientHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(container.clientWidth, container.clientHeight);
  });

  // Animation loop
  (function animate() {
    requestAnimationFrame(animate);
    if (controls) controls.update();
    renderer.render(scene, camera);
  })();
}

// Temperature → color (blue→cyan→green→yellow→red)
function tempToColor(t, tMin, tMax) {
  const u = tMax > tMin ? (t - tMin) / (tMax - tMin) : 0.5;
  const c = new THREE.Color();
  c.setHSL((1 - u) * 0.667, 1.0, 0.5); // 0.667=blue, 0=red
  return c;
}

function fitToView() {
  if (!scene || !camera) return;
  const box = new THREE.Box3();
  scene.traverse(obj => { if (obj.isMesh || obj.isPoints) box.expandByObject(obj); });
  if (box.isEmpty()) return;
  const center = box.getCenter(new THREE.Vector3());
  const size = box.getSize(new THREE.Vector3());
  const maxDim = Math.max(size.x, size.y, size.z);
  camera.position.copy(center).addScalar(maxDim * 1.5);
  camera.lookAt(center);
  if (controls) controls.target.copy(center);
}

document.getElementById('geo-load-btn').addEventListener('click', async () => {
  hideError('geo-alert-area');
  initThreeJS();

  const stlFile = document.getElementById('stl-file').files[0];
  const csvFile = document.getElementById('csv-file').files[0];
  if (!stlFile && !csvFile) {
    showError('geo-alert-area', 'Please select at least one file (STL or CSV).');
    return;
  }

  setLoading('geo-load-btn', true);

  try {
    // Load STL
    if (stlFile) {
      const content = await stlFile.text();
      const data = await apiPost('/api/stl/parse', { content });
      if (!data.valid) throw new Error('STL parse error: ' + data.error_msg);

      if (stlMesh) { scene.remove(stlMesh); stlMesh.geometry.dispose(); }

      const positions = new Float32Array(data.triangles.length * 9);
      data.triangles.forEach((tri, i) => {
        const base = i * 9;
        positions[base]   = tri.v0[0]; positions[base+1] = tri.v0[1]; positions[base+2] = tri.v0[2];
        positions[base+3] = tri.v1[0]; positions[base+4] = tri.v1[1]; positions[base+5] = tri.v1[2];
        positions[base+6] = tri.v2[0]; positions[base+7] = tri.v2[1]; positions[base+8] = tri.v2[2];
      });

      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
      geo.computeVertexNormals();

      const mat = new THREE.MeshPhongMaterial({ color: 0x607090, transparent: true, opacity: 0.7, side: THREE.DoubleSide });
      stlMesh = new THREE.Mesh(geo, mat);
      scene.add(stlMesh);

      const wireGeo = new THREE.WireframeGeometry(geo);
      const wireMat = new THREE.LineBasicMaterial({ color: 0x90aacc, opacity: 0.3, transparent: true });
      scene.add(new THREE.LineSegments(wireGeo, wireMat));

      document.getElementById('stat-tris').textContent = `${data.triangles.length} (${(data.file_size_bytes/1024).toFixed(1)} KB)`;
    }

    // Load CSV
    if (csvFile) {
      const content = await csvFile.text();
      const data = await apiPost('/api/csv/parse', { content });
      if (!data.valid) throw new Error('CSV parse error: ' + data.error_msg);

      if (csvPoints) { scene.remove(csvPoints); csvPoints.geometry.dispose(); }

      csvNodes = data.nodes;
      const temps = csvNodes.map(n => n.temp);
      const tMin = Math.min(...temps), tMax = Math.max(...temps);

      const posArr = new Float32Array(csvNodes.length * 3);
      const colArr = new Float32Array(csvNodes.length * 3);
      csvNodes.forEach((n, i) => {
        posArr[i*3] = n.x; posArr[i*3+1] = n.y; posArr[i*3+2] = n.z;
        const c = tempToColor(n.temp, tMin, tMax);
        colArr[i*3] = c.r; colArr[i*3+1] = c.g; colArr[i*3+2] = c.b;
      });

      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.BufferAttribute(posArr, 3));
      geo.setAttribute('color', new THREE.BufferAttribute(colArr, 3));
      const mat = new THREE.PointsMaterial({ size: 0.04, vertexColors: true });
      csvPoints = new THREE.Points(geo, mat);
      scene.add(csvPoints);

      document.getElementById('geo-statusbar').textContent =
        `CSV: ${csvNodes.length} nodes | T: ${tMin.toFixed(1)}–${tMax.toFixed(1)} °C`;

      // Update legend
      document.getElementById('legend-min').textContent = tMin.toFixed(1) + ' °C';
      document.getElementById('legend-max').textContent = tMax.toFixed(1) + ' °C';
    }

    fitToView();
  } catch (err) {
    showError('geo-alert-area', err.message);
  } finally {
    setLoading('geo-load-btn', false, '🗂 Load Files');
  }
});

document.getElementById('geo-fit-btn').addEventListener('click', fitToView);
document.getElementById('geo-wireframe-btn').addEventListener('click',()=>{if(scene)scene.traverse(o=>{if(o.isLineSegments)o.visible=!o.visible;});});
document.getElementById('geo-reset-btn').addEventListener('click',()=>{if(stlMesh){scene.remove(stlMesh);stlMesh.geometry.dispose();stlMesh=null;}if(csvPoints){scene.remove(csvPoints);csvPoints.geometry.dispose();csvPoints=null;}csvNodes=[];document.getElementById('stat-tris').textContent='?';document.getElementById('stat-nodes').textContent='?';document.getElementById('stat-trange').textContent='?';document.getElementById('temp-legend').style.display='none';});
for(const [inputId,labelId] of [['stl-file','stl-file-info'],['csv-file','csv-file-info']])document.getElementById(inputId).addEventListener('change',e=>{document.getElementById(labelId).textContent=e.target.files[0]?.name||'';});
// ── TAB 2: THERMAL FEA (Task C) ───────────────────────────────────────────────
let feaChart = null;
const feaColors = ['#e94560','#4ecdc4','#f7b731','#a29bfe'];
const feaResults = {};

function buildFEAChart() {
  const ctx = document.getElementById('fea-chart').getContext('2d');
  if (feaChart) feaChart.destroy();
  feaChart = new Chart(ctx, {
    type: 'line',
    data: { datasets: [] },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: 'index', intersect: false },
      plugins: {
        legend: { labels: { color: '#ccc' } },
        tooltip: { callbacks: { label: ctx => `${ctx.dataset.label}: ${ctx.parsed.y.toFixed(4)} °C` } }
      },
      scales: {
        x: { type: 'linear', title: { display: true, text: 'Position x (m)', color: '#aaa' }, ticks: { color: '#aaa' }, grid: { color: '#333' } },
        y: { title: { display: true, text: 'Temperature (°C)', color: '#aaa' }, ticks: { color: '#aaa' }, grid: { color: '#333' } }
      }
    }
  });
}

function addFEADataset(result, nElem, colorIdx) {
  const fePoints = result.x_coords.map((x,i) => ({x, y: result.temperatures[i]}));
  const exactPoints = result.x_coords.map((x,i) => ({x, y: result.temperatures[i]})); // placeholder

  // Exact analytical curve
  const exactData = [];
  for (let i = 0; i <= 100; i++) {
    const xv = i/100;
    exactData.push({ x: xv, y: 100 - 80*xv + (10000/(2*10))*xv*(1-xv) });
  }

  const color = feaColors[colorIdx % feaColors.length];

  feaChart.data.datasets = feaChart.data.datasets.filter(d => d.nElem !== nElem && d.label !== 'Exact');

  feaChart.data.datasets.push({
    label: `N=${nElem} (FE)`,
    data: fePoints,
    borderColor: color,
    backgroundColor: color + '33',
    pointRadius: 4,
    nElem
  });

  // Only add exact once
  const hasExact = feaChart.data.datasets.some(d => d.label === 'Exact');
  if (!hasExact) {
    feaChart.data.datasets.push({
      label: 'Exact',
      data: exactData,
      borderColor: '#ffffff',
      borderDash: [8, 4],
      pointRadius: 0,
      borderWidth: 2
    });
  }
  feaChart.update();
}

async function solveFEA(n) {
  setLoading('fea-solve-btn', true);
  hideError('fea-alert-area');
  try {
    const data = await apiPost('/api/fea/solve', { n_elements: n });
    feaResults[n] = data;
    const idx = [10,20,40,80].indexOf(n);
    if (!feaChart) buildFEAChart();
    addFEADataset(data, n, idx);
    updateFEATable();

    // Enable CSV export
    document.getElementById('fea-export-btn').disabled = false;
  } catch(e) {
    showError('fea-alert-area', 'FEA solver error: ' + e.message);
  } finally {
    setLoading('fea-solve-btn', false, '▶ Solve');
  }
}

function updateFEATable() {
  const tbody = document.getElementById('fea-results-tbody');
  tbody.innerHTML = '';
  [10,20,40,80].forEach(n => {
    if (!feaResults[n]) return;
    const r = feaResults[n];
    const tr = document.createElement('tr');
    const prevN = n/2;
    const conv = feaResults[prevN] ? (feaResults[prevN].max_error / r.max_error).toFixed(2) + '×' : '—';
    tr.innerHTML = `<td>${n}</td><td>${r.max_error.toExponential(4)}</td><td>${conv}</td>`;
    tbody.appendChild(tr);
  });
}

document.getElementById('fea-solve-btn').addEventListener('click', async () => {
  const nSel = document.querySelector('input[name="fea-n"]:checked');
  if (!nSel) { showError('fea-alert-area','Please select N elements.'); return; }
  buildFEAChart();
  if (document.getElementById('fea-run-all').checked) {
    for (const n of [10,20,40,80]) await solveFEA(n);
  } else {
    await solveFEA(parseInt(nSel.value));
  }
});

document.getElementById('fea-export-btn').addEventListener('click', () => {
  let csv = 'N,x_m,T_FE_C,T_exact_C,error_C\n';
  Object.entries(feaResults).forEach(([n, r]) => {
    r.x_coords.forEach((x,i) => {
      const exact = 100 - 80*x + (10000/(2*10))*x*(1-x);
      csv += `${n},${x},${r.temperatures[i].toFixed(6)},${exact.toFixed(6)},${Math.abs(r.temperatures[i]-exact).toExponential(4)}\n`;
    });
  });
  downloadCSV('fea_results.csv', csv);
});

// ── TAB 3: PERFORMANCE (Task D) ───────────────────────────────────────────────
let perfTimeChart = null, perfSpeedChart = null;
const perfData = { workers: [], wallTime: [], speedup: [], efficiency: [] };

function buildPerfCharts() {
  const ctx1 = document.getElementById('perf-time-chart').getContext('2d');
  const ctx2 = document.getElementById('perf-speedup-chart').getContext('2d');
  if (perfTimeChart) perfTimeChart.destroy();
  if (perfSpeedChart) perfSpeedChart.destroy();

  perfTimeChart = new Chart(ctx1, {
    type: 'bar',
    data: {
      labels: [],
      datasets: [{ label: 'Wall Time (ms)', data: [], backgroundColor: '#4ecdc4' }]
    },
    options: {
      responsive: true, maintainAspectRatio: false,
      plugins: { legend: { labels: { color: '#ccc' } } },
      scales: {
        x: { title: { display:true, text:'Workers', color:'#aaa' }, ticks:{color:'#aaa'}, grid:{color:'#333'} },
        y: { title: { display:true, text:'Wall Time (ms)', color:'#aaa' }, ticks:{color:'#aaa'}, grid:{color:'#333'} }
      }
    }
  });

  perfSpeedChart = new Chart(ctx2, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        { label: 'Actual Speedup', data: [], borderColor: '#e94560', pointRadius: 5, tension: 0.3 },
        { label: 'Ideal Speedup', data: [], borderColor: '#aaa', borderDash: [6,3], pointRadius: 0 }
      ]
    },
    options: {
      responsive: true, maintainAspectRatio: false,
      plugins: { legend: { labels: { color: '#ccc' } } },
      scales: {
        x: { title: { display:true, text:'Workers', color:'#aaa' }, ticks:{color:'#aaa'}, grid:{color:'#333'} },
        y: { title: { display:true, text:'Speedup', color:'#aaa' }, ticks:{color:'#aaa'}, grid:{color:'#333'} }
      }
    }
  });
}

function updatePerfCharts() {
  if (!perfTimeChart) return;
  perfTimeChart.data.labels = perfData.workers.map(w => `${w} Worker${w>1?'s':''}`);
  perfTimeChart.data.datasets[0].data = perfData.wallTime;
  perfTimeChart.update();

  perfSpeedChart.data.labels = perfData.workers.map(w => `${w} Worker${w>1?'s':''}`);
  perfSpeedChart.data.datasets[0].data = perfData.speedup;
  perfSpeedChart.data.datasets[1].data = perfData.workers.map(w => w);
  perfSpeedChart.update();
}

async function runPerf(workers) {
  const caseCount = parseInt(document.getElementById('perf-cases').value) || 20;
  const meshSize = parseInt(document.getElementById('perf-mesh').value) || 20;
  const data = await apiPost('/api/parallel/run', { case_count: caseCount, mesh_size: meshSize, num_workers: workers });
  return data;
}

async function runWorkerSet(workerCounts) {
  const tbody=document.getElementById('perf-results-tbody');tbody.innerHTML='';
  perfData.workers=[];perfData.wallTime=[];perfData.speedup=[];perfData.efficiency=[];
  const progress=document.getElementById('perf-progress-wrap');progress.style.display='block';
  for(const requested of workerCounts){
    document.getElementById('perf-progress-label').textContent=`Warm-up and 5 timed repetitions for ${requested} worker(s)?`;
    const r=await runPerf(requested),ms=r.parallel_wall_ms,w=r.worker_count;
    perfData.workers.push(w);perfData.wallTime.push(ms);perfData.speedup.push(r.speedup);perfData.efficiency.push(r.efficiency);
    const tr=document.createElement('tr');tr.innerHTML=`<td>${w}</td><td>${r.serial_wall_ms.toFixed(3)} / ${ms.toFixed(3)}</td><td>${r.speedup.toFixed(3)}</td><td>${r.efficiency.toFixed(3)}</td><td>${r.agreement_ok?'Pass':'Fail'} (${r.max_serial_parallel_diff.toExponential(2)} ?C)</td>`;tbody.appendChild(tr);
  }
  updatePerfCharts();progress.style.display='none';
  const sys=await apiGet('/api/system/info');
  document.getElementById('si-cpus').textContent=sys.logical_cores||'Unavailable';
  document.getElementById('si-hc').textContent=sys.physical_cores||'Unavailable';
  document.getElementById('si-ram').textContent=sys.ram_mb?`${sys.ram_mb} MB`: 'Unavailable';
  document.getElementById('si-os').textContent=`${navigator.platform}; CPU ${sys.cpu}`;
  document.getElementById('si-ua').textContent=`${sys.compiler}; ${sys.build_flags}; seed 42; ${document.getElementById('perf-cases').value} cases; baseline mesh ${document.getElementById('perf-mesh').value}; 1 warm-up + 5 repetitions; median wall time. Table shows serial / parallel ms.`;
}
for(const [id,counts] of [['perf-serial-btn',[1]],['perf-parallel-btn',null],['perf-all-btn',Array.from({length:Math.min(4,navigator.hardwareConcurrency||4)},(_,i)=>i+1)]]){
  document.getElementById(id).addEventListener('click',async()=>{hideError('perf-alert-area');try{const selected=counts||[Number(document.querySelector('input[name="perf-w"]:checked')?.value||2)];await runWorkerSet(selected);}catch(e){document.getElementById('perf-progress-wrap').style.display='none';showError('perf-alert-area','Benchmark error: '+e.message);}});
}
document.getElementById('perf-clear-btn').addEventListener('click',()=>{document.getElementById('perf-results-tbody').innerHTML='<tr><td colspan="5">Run a benchmark to see results</td></tr>';perfData.workers=[];perfData.wallTime=[];perfData.speedup=[];perfData.efficiency=[];updatePerfCharts();});

// ── TAB 4: HARDWARE / EMBEDDED (Task E) ──────────────────────────────────────
let deviceStates = {};

function renderDevices(devices) {
  const list = document.getElementById('hw-device-list');
  list.innerHTML = '';
  devices.forEach(dev => {
    deviceStates[dev.id] = dev;
    const typeIcon = dev.type === 'Microcontroller' ? '🔲' : dev.type === 'FPGA' ? '🔷' : '🔌';
    const statusClass = dev.connected ? 'badge-green' : 'badge-grey';
    const card = document.createElement('div');
    card.className = 'device-card';
    card.id = `device-card-${dev.id}`;
    card.innerHTML = `
      <div class="device-header">
        <span class="device-icon">${typeIcon}</span>
        <span class="device-name">${dev.name}</span>
        <span class="badge ${statusClass}" id="dev-status-${dev.id}">${dev.connected?'Connected':'Disconnected'}</span>
      </div>
      <div class="device-details">
        <span>Type: ${dev.type}</span> | <span>Port: ${dev.port}</span> |
        <span>Voltage: ${dev.voltage}</span> | <span>Protocol: ${dev.protocol}</span>
      </div>
      <div class="device-actions">
        <button class="btn btn-sm btn-primary" onclick="connectDevice(${dev.id})">Connect</button>
        <button class="btn btn-sm btn-secondary" onclick="disconnectDevice(${dev.id})">Disconnect</button>
        <button class="btn btn-sm btn-secondary" onclick="getTelemetry(${dev.id})">Telemetry</button>
        <button class="btn btn-sm btn-secondary" onclick="getCapabilities(${dev.id})">Capabilities</button>
      </div>
      <div class="telemetry-panel" id="telemetry-${dev.id}" style="display:none"></div>
      <div class="error-msg" id="dev-err-${dev.id}" style="display:none"></div>
    `;
    list.appendChild(card);
  });
}

async function connectDevice(id) {
  document.getElementById(`dev-err-${id}`).style.display = 'none';
  const badge = document.getElementById(`dev-status-${id}`);
  badge.textContent = 'Connecting…';
  try {
    const data = await apiPost('/api/embedded/connect', { device_id: id });
    badge.textContent = data.device?.connected ? 'Connected' : 'Disconnected';
    badge.className = `badge ${data.device?.connected ? 'badge-green' : 'badge-grey'}`;
    if (!data.success || data.error) {
      const errEl = document.getElementById(`dev-err-${id}`);
      errEl.textContent = `⚠ ${data.error} — Click Connect again to retry.`;
      errEl.style.display = 'block';
    }
  } catch(e) {
    badge.textContent = 'Error';
    badge.className = 'badge badge-red';
    const errEl = document.getElementById(`dev-err-${id}`);
    errEl.textContent = e.message;
    errEl.style.display = 'block';
  }
}

async function disconnectDevice(id) {
  const badge = document.getElementById(`dev-status-${id}`);
  try {
    const data = await apiPost('/api/embedded/disconnect', { device_id: id });
    badge.textContent = 'Disconnected';
    badge.className = 'badge badge-grey';
  } catch(e) {
    badge.textContent = 'Error'; badge.className = 'badge badge-red';
  }
}

async function getTelemetry(id) {
  const panel = document.getElementById(`telemetry-${id}`);
  try {
    const data = await apiPost('/api/embedded/telemetry', { device_id: id });
    panel.style.display = 'block';
    const t=data.telemetry;
    panel.innerHTML = `<strong>Simulated telemetry @ ${t.timestamp}</strong><br>Temperature: ${t.temperature_C.toFixed(1)} \u00b0C | Voltage: ${t.voltage_V.toFixed(3)} V | ${t.message}`;
  } catch(e) {
    panel.style.display = 'block';
    panel.innerHTML = `<span style="color:#e94560">Telemetry error: ${e.message}</span>`;
  }
}

async function getCapabilities(id) {
  const panel=document.getElementById(`telemetry-${id}`);
  try {
    const data=await apiPost('/api/embedded/capabilities',{device_id:id});
    panel.style.display='block';
    panel.textContent=`Simulated capabilities: ${JSON.stringify(data.capabilities)}`;
  } catch(e) { panel.style.display='block';panel.textContent=`Capability error: ${e.message}`; }
}

document.getElementById('hw-refresh-btn').addEventListener('click',()=>document.getElementById('hw-discover-btn').click());
document.getElementById('hw-discover-btn').addEventListener('click', async () => {
  hideError('hw-alert-area');
  setLoading('hw-discover-btn', true);
  try {
    const data = await apiGet('/api/embedded/devices');
    renderDevices(data.devices || []);
  } catch(e) {
    showError('hw-alert-area', 'Discovery error: ' + e.message);
  } finally {
    setLoading('hw-discover-btn', false, '🔍 Discover Devices');
  }
});

// ── TAB 5: BACKPROP (Task F) ──────────────────────────────────────────────────
let bpChart = null;
function buildBPChart() {
  const ctx=document.getElementById('bp-loss-chart').getContext('2d');
  if(bpChart)bpChart.destroy();
  bpChart=new Chart(ctx,{type:'line',data:{datasets:[{label:'Training loss J',data:[],borderColor:'#e94560',pointRadius:0}]},options:{responsive:true,maintainAspectRatio:false,scales:{x:{type:'linear',title:{display:true,text:'Step'}},y:{title:{display:true,text:'Loss J'}}}}});
}
function bpParams(){return {w:Number(document.getElementById('bp-w').value),b:Number(document.getElementById('bp-b').value),v:Number(document.getElementById('bp-v').value),c:Number(document.getElementById('bp-c').value),lr:Number(document.getElementById('bp-lr').value),steps:Number(document.getElementById('bp-steps').value)};}
async function runBackprop(){const p=bpParams();if(![p.w,p.b,p.v,p.c,p.lr,p.steps].every(Number.isFinite)||p.lr<=0||p.steps<1||p.steps>2000)throw new Error('Enter finite parameters, a positive learning rate, and 1?2000 steps.');return apiPost('/api/backprop/run',p);}
document.getElementById('bp-forward-btn').addEventListener('click',async()=>{try{const d=await runBackprop();document.getElementById('bp-forward-display').style.display='block';document.getElementById('bp-forward-math').textContent=`z=${d.forward.z.toFixed(6)}, h=tanh(z)=${d.forward.h.toFixed(6)}, prediction=${d.forward.y_hat.toFixed(6)}, loss=${d.forward.loss.toFixed(8)} (x=1, target=2)`;}catch(e){showError('bp-error',e.message);}});
document.getElementById('bp-backward-btn').addEventListener('click',async()=>{try{const d=await runBackprop(),g=d.gradients,p=bpParams();document.getElementById('bp-backward-display').style.display='block';document.getElementById('bp-backward-math').textContent=`dJ/dw=${g.dJ_dw.toFixed(8)}, dJ/db=${g.dJ_db.toFixed(8)}, dJ/dv=${g.dJ_dv.toFixed(8)}, dJ/dc=${g.dJ_dc.toFixed(8)}`;document.getElementById('bp-update-math').textContent=`Simultaneous update (?=${p.lr}): w=${(p.w-p.lr*g.dJ_dw).toFixed(8)}, b=${(p.b-p.lr*g.dJ_db).toFixed(8)}, v=${(p.v-p.lr*g.dJ_dv).toFixed(8)}, c=${(p.c-p.lr*g.dJ_dc).toFixed(8)}`;}catch(e){showError('bp-error',e.message);}});
document.getElementById('bp-gradcheck-btn').addEventListener('click',async()=>{try{const d=await runBackprop(),body=document.getElementById('bp-gradcheck-tbody');body.innerHTML='';d.grad_check.forEach(g=>{const tr=document.createElement('tr');tr.innerHTML=`<td>${g.param}</td><td>${g.analytic.toFixed(9)}</td><td>${g.numeric.toFixed(9)}</td><td>${g.abs_diff.toExponential(2)}</td><td>${g.abs_diff<=1e-5?'Pass':'Fail'}</td>`;body.appendChild(tr);});document.getElementById('bp-gradcheck-display').style.display='block';}catch(e){showError('bp-error',e.message);}});
document.getElementById('bp-train-btn').addEventListener('click',async()=>{try{const d=await runBackprop();if(!bpChart)buildBPChart();bpChart.data.datasets[0].data=d.loss_history.map(([step,loss])=>({x:step,y:loss}));bpChart.update();document.getElementById('bp-train-info').style.display='block';document.getElementById('bp-train-step').textContent=d.loss_history.length-1;document.getElementById('bp-train-loss-val').textContent=d.loss_history.at(-1)[1].toExponential(4);}catch(e){showError('bp-error',e.message);}});
document.getElementById('bp-train-clear-btn').addEventListener('click',()=>{if(bpChart){bpChart.data.datasets[0].data=[];bpChart.update();}});
document.getElementById('bp-reset-btn').addEventListener('click',()=>[['bp-w','0.5'],['bp-b','0'],['bp-v','1'],['bp-c','0'],['bp-lr','0.1'],['bp-steps','100']].forEach(([id,v])=>document.getElementById(id).value=v));

document.addEventListener('DOMContentLoaded', () => {
  showTab('geo-tab');
  buildFEAChart();
  buildPerfCharts();
  buildBPChart();
  const statusDot = document.getElementById('srv-dot');
  const statusText = document.getElementById('srv-text');
  apiGet('/api/health').then(() => {
    statusDot.classList.remove('offline');
    statusDot.classList.add('online');
    statusText.textContent = 'Server online';
  }).catch(() => {
    statusDot.classList.remove('online');
    statusDot.classList.add('offline');
    statusText.textContent = 'Server offline';
  });
  // Auto-discover devices on hardware tab click
  document.querySelector('[data-tab="hw-tab"]').addEventListener('click', () => {
    setTimeout(async () => {
      try {
        const data = await apiGet('/api/embedded/devices');
        renderDevices(data.devices || []);
      } catch(e) { /* silent on auto-discover */ }
    }, 200);
  });
});
