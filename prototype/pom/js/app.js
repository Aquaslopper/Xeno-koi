// Xeno Koi — live POM preview.
// Draws one full-screen quad through shaders/pom.frag, which is the game's
// background shader (assets/depth_fog.fs) with a parallax ray march in front of it.

const BACKGROUNDS = [
  { id: "harvest", label: "HARVEST", file: "../../assets/harvest.jpg"     },
  { id: "hole",    label: "HOLE",    file: "../../assets/hole.jpg"        },
  { id: "arctic",  label: "ARCTIC",  file: "../../assets/arctic.jpg"      },
  { id: "castle",  label: "CASTLE",  file: "../../assets/castle.jpg"      },
  { id: "city",    label: "CITY",    file: "../../assets/city.jpg"        },
  { id: "deus",    label: "DEUS",    file: "../../assets/deus.png"        },
  { id: "face",    label: "FACE",    file: "../../assets/face.jpg"        },
  { id: "realm",   label: "REALM",   file: "../../assets/realm.jpg"       },
];

const canvas = document.getElementById("gl");
const gl = canvas.getContext("webgl2", { antialias: false, alpha: false, powerPreference: "high-performance" });
if (!gl) {
  document.body.innerHTML = "<p style='padding:2rem;color:#64f0ff;font:14px monospace'>" +
    "This preview needs WebGL2. The game's own WASM build has the same requirement for its shader path.</p>";
  throw new Error("no webgl2");
}

// ------------------------------------------------------------------ state
const state = {
  bg: 0,
  depth: 14,
  steps: 28,
  ao: 0.40,
  shadow: 1.0,
  wipe: 0,
  light: 1,
  fog: 1,
  table: 0,
  pointerCam: 1,
  vantage: [0.5, 0.5],
  target: [0.5, 0.5],
  frozen: false,
};

// ------------------------------------------------------------------ gl plumbing
function fail(msg) {
  const b = document.createElement("pre");
  b.style.cssText = "position:fixed;left:24px;bottom:24px;max-width:60ch;z-index:99;padding:14px 16px;" +
    "background:rgba(40,0,0,.92);border:1px solid #ff5f5f;color:#ffd9d9;font:11px/1.5 monospace;white-space:pre-wrap";
  b.textContent = msg;
  document.body.appendChild(b);
  console.error(msg);
}

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
  vUV = vec2(aPos.x * 0.5 + 0.5, 0.5 - aPos.y * 0.5);  // y = 0 at the top, like the game's fragTexCoord
  gl_Position = vec4(aPos, 0.0, 1.0);
}`;

let prog, uni = {};
const quad = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, quad);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);

const tex = { color: gl.createTexture(), normal: gl.createTexture(), height: gl.createTexture() };

function makeTex(unit, filter) {
  const t = [tex.color, tex.normal, tex.height][unit];
  gl.activeTexture(gl.TEXTURE0 + unit);
  gl.bindTexture(gl.TEXTURE_2D, t);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
}

async function loadTexture(unit, url, filter = gl.LINEAR) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`cannot load ${url}`);
  const bmp = await createImageBitmap(await res.blob(), { imageOrientation: "none" });
  gl.activeTexture(gl.TEXTURE0 + unit);
  gl.bindTexture(gl.TEXTURE_2D, [tex.color, tex.normal, tex.height][unit]);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, bmp);
  makeTex(unit, filter);
  bmp.close?.();
}

async function init() {
  const fragSrc = await (await fetch("shaders/pom.frag")).text();
  prog = gl.createProgram();
  gl.attachShader(prog, compile(gl.VERTEX_SHADER, VS));
  gl.attachShader(prog, compile(gl.FRAGMENT_SHADER, fragSrc));
  gl.bindAttribLocation(prog, 0, "aPos");
  gl.linkProgram(prog);
  if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(prog));

  for (const n of ["uColor", "uNormal", "uHeight", "uRes", "uTime", "uFlash", "uLight", "uFog",
                   "uVantage", "uDepth", "uSteps", "uPom", "uAO", "uShadow", "uWipe"]) {
    uni[n] = gl.getUniformLocation(prog, n);
  }
  gl.useProgram(prog);
  gl.uniform1i(uni.uColor, 0);
  gl.uniform1i(uni.uNormal, 1);
  gl.uniform1i(uni.uHeight, 2);
  gl.uniform2f(uni.uRes, canvas.width, canvas.height);
  gl.uniform1f(uni.uFlash, 0.0);

  buildChips();
  await loadBackground(0);
  requestAnimationFrame(frame);
}

async function loadBackground(i) {
  state.bg = i;
  const bg = BACKGROUNDS[i];
  const btn = document.querySelectorAll("#bgChips .chip")[i];
  document.querySelectorAll("#bgChips .chip").forEach((b) => b.classList.remove("on"));
  btn?.classList.add("on");
  document.getElementById("bgName").textContent = bg.label;
  await Promise.all([
    loadTexture(0, bg.file, gl.LINEAR),
    loadTexture(1, `../../assets/${bg.id}_normal.png`, gl.LINEAR),
    loadTexture(2, `height/${bg.id}_height.png`, gl.LINEAR),
  ]);
}

// ------------------------------------------------------------------ UI
function buildChips() {
  const box = document.getElementById("bgChips");
  BACKGROUNDS.forEach((bg, i) => {
    const b = document.createElement("button");
    b.className = "chip" + (i === 0 ? " on" : "");
    b.textContent = bg.label;
    b.onclick = () => loadBackground(i);
    box.appendChild(b);
  });
}

const bind = (id, key, out, fmt = (v) => v) => {
  const el = document.getElementById(id);
  const o = document.getElementById(out);
  const write = () => { o.textContent = fmt(parseFloat(el.value)); };
  el.addEventListener("input", () => { state[key] = parseFloat(el.value); write(); });
  write();
  return el;
};

bind("depth", "depth", "vDepth", (v) => `${v.toFixed(0)} px`);
bind("steps", "steps", "vSteps", (v) => `${v.toFixed(0)}`);
bind("ao", "ao", "vAO", (v) => v.toFixed(2));
bind("shadow", "shadow", "vShadow", (v) => v.toFixed(2));
bind("wipe", "wipe", "vWipe",
     (v) => (v <= 0.001 ? "OFF · FULL POM" : v >= 0.999 ? "FULLY FLAT" : `FLAT ${(v * 100).toFixed(0)}% / POM ${(100 - v * 100).toFixed(0)}%`));

document.getElementById("tLight").onclick = (e) => { state.light ^= 1; e.target.classList.toggle("on", !!state.light); };
document.getElementById("tFog").onclick = (e) => { state.fog ^= 1; e.target.classList.toggle("on", !!state.fog); };
document.getElementById("tTable").onclick = (e) => {
  state.table ^= 1;
  e.target.classList.toggle("on", !!state.table);
  document.getElementById("table").style.display = state.table ? "block" : "none";
};
document.getElementById("tCam").onclick = (e) => { state.pointerCam ^= 1; e.target.classList.toggle("on", !!state.pointerCam); };

canvas.addEventListener("pointermove", (ev) => {
  const r = canvas.getBoundingClientRect();
  state.target[0] = (ev.clientX - r.left) / r.width;
  state.target[1] = (ev.clientY - r.top) / r.height;
});
canvas.addEventListener("pointerleave", () => { state.target = [0.5, 0.5]; });

const panel = document.getElementById("panel");
addEventListener("keydown", (ev) => {
  if (ev.code === "Space") { ev.preventDefault(); state.frozen = true; }
  if (ev.key === "h" || ev.key === "H") panel.classList.toggle("hidden");
  if (ev.key === "r" || ev.key === "R") { state.target = [0.5, 0.5]; state.vantage = [0.5, 0.5]; }
  const n = parseInt(ev.key, 10);
  if (n >= 1 && n <= 8) loadBackground(n - 1);
});
addEventListener("keyup", (ev) => { if (ev.code === "Space") state.frozen = false; });

// wipe divider drawn over the canvas
const line = document.getElementById("wipeLine");
const tags = document.querySelectorAll("#wipeTag");
function drawWipeUI() {
  if (state.wipe <= 0.001 || state.wipe >= 0.999) { line.style.display = "none"; tags.forEach(t => t.style.display = "none"); return; }
  line.style.display = "block";
  line.style.left = `${state.wipe * 100}%`;
  tags.forEach((t, i) => { t.style.display = "block"; t.style.opacity = "1"; });
  tags[1].style.display = "block";
}

// ------------------------------------------------------------------ frame
let last = performance.now(), acc = 0, frames = 0;
const fpsEl = document.getElementById("fps");
const costEl = document.getElementById("cost");
const t0 = performance.now() / 1000;

function frame(now) {
  const dt = Math.min((now - last) / 1000, 0.1);
  last = now;
  const t = now / 1000 - t0;

  // critically damped follow so the eye feels heavy, like the rest of the game
  const k = 1 - Math.exp(-dt * 3.4);
  if (!state.pointerCam) {
    // no pointer: the eye breathes, slowly, so the room is never quite still
    state.target[0] = 0.5 + Math.sin(t * 0.107) * 0.055;
    state.target[1] = 0.5 + Math.cos(t * 0.083) * 0.030;
  }
  state.vantage[0] += (state.target[0] - state.vantage[0]) * k;
  state.vantage[1] += (state.target[1] - state.vantage[1]) * k;

  const pom = state.frozen ? 0 : 1;
  gl.viewport(0, 0, canvas.width, canvas.height);
  gl.useProgram(prog);
  gl.uniform1f(uni.uTime, t);
  gl.uniform1f(uni.uLight, state.light ? 1 : 0);
  gl.uniform1f(uni.uFog, state.fog ? 1 : 0);
  gl.uniform2f(uni.uVantage, state.vantage[0], state.vantage[1]);
  gl.uniform1f(uni.uDepth, state.depth);
  gl.uniform1i(uni.uSteps, state.steps);
  gl.uniform1f(uni.uPom, pom);
  gl.uniform1f(uni.uAO, state.ao);
  gl.uniform1f(uni.uShadow, state.shadow);
  gl.uniform1f(uni.uWipe, state.wipe > 0.999 ? 1.001 : state.wipe);

  gl.bindBuffer(gl.ARRAY_BUFFER, quad);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
  gl.drawArrays(gl.TRIANGLES, 0, 3);

  drawWipeUI();

  frames++;
  acc += dt;
  if (acc > 0.4) {
    fpsEl.textContent = (frames / acc).toFixed(0);
    const bloomTaps = 8, shadowTaps = state.ao > 0.01 || state.shadow > 0.01 ? 8 : 0;
    const taps = state.steps * 1 + bloomTaps + shadowTaps + 3;
    costEl.textContent = `≈ ${taps} texture fetches / pixel`;
    frames = 0; acc = 0;
  }
  requestAnimationFrame(frame);
}

init().catch((e) => fail("POM preview failed to start\n\n" + (e && e.message ? e.message : e)));
