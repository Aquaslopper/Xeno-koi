// Live preview for the three safe uses of the height maps.
// One full-screen quad through shaders/ambient.frag; the fragment shader is the game's
// background pipeline plus a height term, never a changed sampling point.

const BACKGROUNDS = [
  { id: "harvest", label: "HRVST", file: "../../assets/harvest.jpg" },
  { id: "hole",    label: "HOLE",  file: "../../assets/hole.jpg"    },
  { id: "arctic",  label: "ARCTC", file: "../../assets/arctic.jpg"  },
  { id: "castle",  label: "CSTLE", file: "../../assets/castle.jpg"  },
  { id: "city",    label: "CITY",  file: "../../assets/city.jpg"    },
  { id: "deus",    label: "DEUS",  file: "../../assets/deus.png"    },
  { id: "face",    label: "FACE",  file: "../../assets/face.jpg"    },
  { id: "realm",   label: "REALM", file: "../../assets/realm.jpg"   },
];

const NOTES = {
  fog: "The same fog, weighted by height so it pools in the low ground and leaves the ridges clear. Nothing moves.",
  beam: "The drifting light is blocked by whatever stands between it and the pixel. Drag BEAM ANGLE to sweep it by hand.",
  bake: "The occlusion that would be baked into the JPEG offline. Slider decides the strength to export at; nothing changes at runtime.",
};

const canvas = document.getElementById("gl");
const gl = canvas.getContext("webgl2", { antialias: false, alpha: false, powerPreference: "high-performance" });

function fail(msg) {
  const b = document.createElement("pre");
  b.style.cssText = "position:fixed;left:24px;bottom:24px;max-width:64ch;z-index:99;padding:14px 16px;" +
    "background:rgba(40,0,0,.94);border:1px solid #ff5f5f;color:#ffd9d9;font:11px/1.5 monospace;white-space:pre-wrap";
  b.textContent = msg;
  document.body.appendChild(b);
  console.error(msg);
}
if (!gl) {
  fail("This preview needs WebGL2.");
  throw new Error("no webgl2");
}

const state = {
  bg: 3,          // castle
  mode: 0,        // 0 fog, 1 beam, 2 bake
  strength: 1.0,
  mix: 1.0,
  phase: -1,      // -1 = follow the clock
  freeze: false,
  table: false,
  clamp: true,
  pointer: [0.5, 0.5],
};

// ------------------------------------------------------------------ gl
function compile(type, src) {
  const s = gl.createShader(type);
  gl.shaderSource(s, src);
  gl.compileShader(s);
  if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
    throw new Error((type === gl.VERTEX_SHADER ? "vertex" : "fragment") +
                    " shader:\n" + gl.getShaderInfoLog(s));
  }
  return s;
}

const VS = `#version 300 es
in vec2 aPos;
out vec2 vUV;
void main() {
  vUV = vec2(aPos.x * 0.5 + 0.5, 0.5 - aPos.y * 0.5);   // y = 0 at the top
  gl_Position = vec4(aPos, 0.0, 1.0);
}`;

let prog;
const uni = {};
const quad = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, quad);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);

const textures = [gl.createTexture(), gl.createTexture(), gl.createTexture()];

function bindTex(unit) {
  gl.activeTexture(gl.TEXTURE0 + unit);
  gl.bindTexture(gl.TEXTURE_2D, textures[unit]);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
}

async function loadTexture(unit, url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`cannot load ${url} (${res.status})`);
  const bmp = await createImageBitmap(await res.blob(), { imageOrientation: "none" });
  bindTex(unit);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, bmp);
  bindTex(unit);
  bmp.close?.();
}

async function init() {
  const frag = await (await fetch("shaders/ambient.frag")).text();
  prog = gl.createProgram();
  gl.attachShader(prog, compile(gl.VERTEX_SHADER, VS));
  gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, frag));
  gl.bindAttribLocation(prog, 0, "aPos");
  gl.linkProgram(prog);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(prog));

  for (const n of ["uColor", "uNormal", "uHeight", "uRes", "uTime", "uLight", "uFog",
                   "uVantage", "uMode", "uStrength", "uMix", "uPhase", "uClamp"]) {
    uni[n] = gl.getUniformLocation(prog, n);
  }
  gl.useProgram(prog);
  gl.uniform1i(uni.uColor, 0);
  gl.uniform1i(uni.uNormal, 1);
  gl.uniform1i(uni.uHeight, 2);
  gl.uniform2f(uni.uRes, canvas.width, canvas.height);
  gl.uniform1f(uni.uLight, 1);
  gl.uniform1f(uni.uFog, 1);

  buildChips();
  setMode(0);
  await loadBackground(3);
  requestAnimationFrame(frame);
}

async function loadBackground(i) {
  state.bg = i;
  const bg = BACKGROUNDS[i];
  document.querySelectorAll("#bgChips .chip").forEach((b, j) => b.classList.toggle("on", j === i));
  document.getElementById("bgName").textContent = bg.id.toUpperCase();
  await Promise.all([
    loadTexture(0, bg.file),
    loadTexture(1, `../../assets/${bg.id}_normal.png`),
    loadTexture(2, `height/${bg.id}_height.png`),
  ]);
}

// ------------------------------------------------------------------ ui
function buildChips() {
  const box = document.getElementById("bgChips");
  BACKGROUNDS.forEach((bg, i) => {
    const b = document.createElement("button");
    b.className = "chip" + (i === state.bg ? " on" : "");
    b.textContent = bg.label;
    b.title = bg.id;
    b.onclick = () => loadBackground(i);
    box.appendChild(b);
  });
}

const MODE_NAMES = ["A · FOG SETTLES LOW", "B · BEAM OCCLUSION", "C · BAKED AO"];

function setMode(m) {
  state.mode = m;
  document.querySelectorAll("[data-mode]").forEach((b) =>
    b.classList.toggle("on", Number(b.dataset.mode === "fog" ? 0 : b.dataset.mode === "beam" ? 1 : 2) === m));
  document.getElementById("modeName").textContent = MODE_NAMES[m];
  document.getElementById("note").textContent = NOTES[["fog", "beam", "bake"][m]];

  const beamRow = document.getElementById("rowPhase");
  const beamSlider = document.getElementById("phase");
  const showBeam = m === 1;
  beamRow.style.display = showBeam ? "flex" : "none";
  beamSlider.style.display = showBeam ? "block" : "none";
}

document.querySelectorAll("[data-mode]").forEach((b) => {
  b.onclick = () => setMode(b.dataset.mode === "fog" ? 0 : b.dataset.mode === "beam" ? 1 : 2);
});

function slider(id, key, out, fmt) {
  const el = document.getElementById(id);
  const o = document.getElementById(out);
  const write = () => { o.textContent = fmt(parseFloat(el.value)); };
  el.addEventListener("input", () => { state[key] = parseFloat(el.value); write(); });
  write();
  return el;
}

slider("mix", "mix", "vMix", (v) => `${(v * 100).toFixed(0)}%`);
slider("strength", "strength", "vStrength", (v) => v.toFixed(2));

const phaseEl = document.getElementById("phase");
const phaseOut = document.getElementById("vPhase");
function setPhase(v) {
  state.phase = v < 0 ? -1 : v;
  phaseEl.value = v < 0 ? 0.25 : v;
  phaseOut.textContent = v < 0 ? "AUTO (CLOCK)" : `${(v * 360).toFixed(0)}°`;
}
phaseEl.addEventListener("input", () => setPhase(parseFloat(phaseEl.value)));
setPhase(-1);

document.getElementById("tTable").onclick = (e) => {
  state.table ^= 1;
  e.currentTarget.classList.toggle("on", !!state.table);
  drawTable(state.table);
};
document.getElementById("tClamp").onclick = (e) => {
  state.clamp ^= 1;
  e.currentTarget.classList.toggle("on", !!state.clamp);
};
document.getElementById("tFreeze").onclick = (e) => {
  state.freeze ^= 1;
  e.currentTarget.classList.toggle("on", !!state.freeze);
};

canvas.addEventListener("pointermove", (ev) => {
  const r = canvas.getBoundingClientRect();
  state.pointer[0] = (ev.clientX - r.left) / r.width;
  state.pointer[1] = (ev.clientY - r.top) / r.height;
});

const panel = document.getElementById("panel");
addEventListener("keydown", (ev) => {
  if (ev.code === "Space") { ev.preventDefault(); state.mix = 0; document.getElementById("mix").value = 0; }
  if (ev.key === "h" || ev.key === "H") panel.classList.toggle("hidden");
  if (ev.key === "q" || ev.key === "Q") setMode(0);
  if (ev.key === "w" || ev.key === "W") setMode(1);
  if (ev.key === "e" || ev.key === "E") setMode(2);
  const n = parseInt(ev.key, 10);
  if (n >= 1 && n <= 8) loadBackground(n - 1);
});
addEventListener("keyup", (ev) => {
  if (ev.code === "Space") { state.mix = 1; document.getElementById("mix").value = 1; }
});

// the board, at the real geometry, as a translucent overlay
const TABLE_CARDS = [
  [45.3, 1.7, 4.4, 12.6], [51.5, 1.7, 4.4, 12.6], [57.7, 1.7, 4.4, 12.6], [63.9, 1.7, 4.4, 12.6],
  [40.0, 23.6, 5.9, 17.2], [47.5, 23.6, 5.9, 17.2], [55.0, 23.6, 5.9, 17.2],
  [40.0, 43.1, 5.9, 17.2], [47.5, 43.1, 5.9, 17.2], [55.0, 43.1, 5.9, 17.2],
  [48.0, 78.6, 6.9, 19.9], [55.5, 78.6, 6.9, 19.9], [63.0, 78.6, 6.9, 19.9],
];
function drawTable(on) {
  let box = document.getElementById("tableOverlay");
  if (!on) { box?.remove(); return; }
  box = document.createElement("div");
  box.id = "tableOverlay";
  box.style.cssText = "position:absolute;inset:0;pointer-events:none;";
  box.innerHTML =
    '<div style="position:absolute;inset:0;background:rgba(0,0,0,.15)"></div>' +
    '<div style="position:absolute;left:0.94%;top:41.4%;width:20%;height:48.9%;border:1px solid rgba(100,240,255,.18);background:rgba(0,0,0,.55)"></div>' +
    '<div style="position:absolute;left:90.6%;top:6%;width:7.8%;height:86%;border:1px solid rgba(100,240,255,.16);background:rgba(0,0,0,.6)"></div>' +
    '<div style="position:absolute;left:25.8%;top:31.4%;width:5.9%;height:17.2%;border:1px solid rgba(100,240,255,.25);background:rgba(0,0,0,.6)"></div>';
  for (const [x, y, w, h] of TABLE_CARDS) {
    box.innerHTML += `<div style="position:absolute;left:${x}%;top:${y}%;width:${w}%;height:${h}%;` +
      `border:1px solid rgba(100,240,255,.22);background:rgba(0,0,0,.5);border-radius:3px"></div>`;
  }
  document.getElementById("wrap").appendChild(box);
}

// ------------------------------------------------------------------ frame
let last = performance.now();
const t0 = last / 1000;
const readout = document.getElementById("readout");

function frame(now) {
  const t = now / 1000 - t0;

  gl.viewport(0, 0, canvas.width, canvas.height);
  gl.useProgram(prog);
  gl.uniform1f(uni.uTime, state.freeze ? 0.0 : t);
  gl.uniform2f(uni.uVantage, state.pointer[0], state.pointer[1]);
  gl.uniform1i(uni.uMode, state.mode);
  gl.uniform1f(uni.uStrength, state.strength);
  gl.uniform1f(uni.uMix, state.mix);
  gl.uniform1f(uni.uPhase, state.phase);
  gl.uniform1f(uni.uClamp, state.clamp ? 1 : 0);

  gl.bindBuffer(gl.ARRAY_BUFFER, quad);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
  gl.drawArrays(gl.TRIANGLES, 0, 3);

  readout.textContent = state.mode === 0 ? "runtime cost: +1 fetch/px"
    : state.mode === 1 ? "runtime cost: ≈ 12 fetch/px"
    : "runtime cost: 0 — this is what the export would look like";

  requestAnimationFrame(frame);
}

init().catch((e) => fail("Preview failed to start\n\n" + (e && e.message ? e.message : e)));
