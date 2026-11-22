// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/airtraffic/), adapted to GLSL 3.30 only by the prelude files.

// Antialiased box: 1 inside |p - c| < h.
float at_box(vec2 p, vec2 c, vec2 h, float fw) {
  vec2 d = abs(p - c) - h;
  float sd = max(d.x, d.y);
  fw = max(fw, 1e-4);
  return 1.0 - smoothstep(-fw, fw, sd);
}
// A grid of windows, each lit or dark by hash; fades to its average when a cell is under two pixels.
float at_windows(vec2 uv, vec2 cell, vec2 fill, float seed, float litPart, vec2 fw) {
  vec2 g = uv / cell;
  vec2 id = floor(g);
  vec2 f = fract(g);
  vec2 e = max(fw / cell, vec2(1e-4));
  vec2 lo = 0.5 * (1.0 - fill);
  vec2 m = smoothstep(lo - e, lo + e, f) * (1.0 - smoothstep(1.0 - lo - e, 1.0 - lo + e, f));
  float lit = step(1.0 - litPart, sc_hash21(id + seed));
  float detail = sc_detail(min(cell.x, cell.y), max(fw.x, fw.y));
  return mix(litPart * fill.x * fill.y, lit * m.x * m.y, detail);
}
vec3 at_palette(float index) {
  float i = floor(index);
  return mix(sc_neon(i), sc_neon(i + 1.0), index - i);
}
// Two quick flashes at the start of a cycle of phase 0..1.
float at_flash(float ph) {
  return step(ph, 0.07) + step(0.15, ph) * step(ph, 0.22);
}
// Face normal from the screen-space derivatives of the position, turned towards the viewer. Never
// NaN: a NaN written by an opaque surface is spread into a black block by the bloom.
vec3 at_normal(vec3 dx, vec3 dy, vec3 view) {
  vec3 n = cross(dx, dy);
  n /= max(length(n), 1e-20);
  return dot(n, view) > 0.0 ? -n : n;
}
float at_fresnel(vec3 n, vec3 view) {
  float f = clamp(1.0 - abs(dot(n, view)), 0.0, 1.0);
  return f * f * f;
}
// Unlit hull: deep indigo, a little sky from above, a little city glow from below, a fog-coloured rim.
vec3 at_hullBase(vec3 n, vec3 view) {
  float skyAmt = smoothstep(-0.5, 0.7, n.y);
  vec3 amb = mix(uFogWarm * 0.6 + uAmbient * 0.6, uSkyMid * 0.7 + uAmbient, skyAmt);
  float fres = at_fresnel(n, view);
  vec3 col = vec3(0.007, 0.006, 0.018) + amb * 0.16 + fres * sc_fogColor(view) * 0.22;
  col += pow(max(dot(n, uSunDir), 0.0), 2.0) * uSunBottom * 0.012;
  return col;
}

varying vec3 vP;
varying vec3 vLocal;
flat varying float vPart;
flat varying vec4 vRun;
flat varying vec3 vRight;
flat varying vec3 vFwd;

// The airship's flank screen: a striped sun over a neon grid, and an equaliser driven by the beat.
// p: metres on the panel, x -50..50 (reading direction), y -8.5..12.5.
vec3 at_screen(vec2 p, float seed) {
  float v = (p.y + 8.5) / 21.0;
  vec3 colr = mix(sc_neon(0.0), sc_neon(2.0), smoothstep(0.25, 1.0, v)) * mix(0.42, 0.1, v);
  // Sun.
  vec2 sp = p - vec2(-24.0, 1.5);
  float sr = length(sp);
  float sun = 1.0 - smoothstep(8.4, 9.0, sr);
  float cut = step(sp.y, 1.0) * step(fract((sp.y - uTime * 0.7) / 2.4), 0.36 - 0.03 * sp.y);
  vec3 sunCol = mix(sc_neon(0.0), sc_neon(4.0), smoothstep(-8.0, 7.0, sp.y)) * 2.1;
  colr = mix(colr, sunCol, sun * (1.0 - cut));
  // Grid floor under the horizon.
  float below = step(p.y, -3.6);
  float depth = 1.0 / max((-3.6 - p.y) / 4.9 + 0.04, 0.04);
  float rows = step(fract(depth * 0.9 - uTime * 0.5), 0.3);
  float cols = step(abs(fract((p.x + 24.0) * depth * 0.035 + 0.5) - 0.5), 0.07 + 0.02 * depth);
  colr = mix(colr, sc_neon(1.0) * 1.7, below * max(rows, cols) * step(p.x, -2.0));
  colr *= 1.0 - 0.75 * below * step(-2.0, p.x) * step(p.x, 0.0);
  // Equaliser.
  float eq = step(2.0, p.x) * step(p.x, 47.0);
  float bar = floor((p.x - 2.0) / 3.0);
  float beat = floor(uBeat.z);
  float h0 = sc_hash21(vec2(bar, beat + seed * 64.0));
  float h1 = sc_hash21(vec2(bar, beat + 1.0 + seed * 64.0));
  float level = mix(h1, h0, uBeat.x) * (0.45 + 0.55 * uBeat.w) * 0.85 + 0.12;
  float gap = step(0.2, fract((p.x - 2.0) / 3.0));
  float on = eq * gap * step(v, level);
  vec3 barCol = mix(sc_neon(1.0), sc_neon(0.0), smoothstep(0.2, 0.8, v)) * 1.9;
  colr = mix(colr, barCol, on);
  colr = mix(colr, sc_neon(7.0) * 2.4, eq * gap * step(abs(v - level - 0.035), 0.022));
  return colr;
}

void main() {
  // Derivatives first, before anything branches.
  vec3 dx = dFdx(vP), dy = dFdy(vP);
  vec3 fl = fwidth(vLocal);
  vec3 L = vLocal;
  float fwm = max(fl.x, max(fl.y, fl.z));

  vec3 view = normalize(vP - vec3(0.0, cameraPosition.y, 0.0));
  vec3 n = at_normal(dx, dy, view);
  vec3 nl = vec3(dot(n, vRight), n.y, -dot(n, vFwd));

  float type = vRun.w;
  float seed = floor(vRun.z * 4096.0);
  vec3 white = sc_neon(7.0);
  vec3 warm = mix(sc_neon(3.0), sc_neon(4.0), 0.55);
  float side = smoothstep(0.45, 0.75, abs(nl.x));
  float top = smoothstep(0.5, 0.8, nl.y);
  float under = smoothstep(0.3, 0.65, -nl.y);
  float rear = smoothstep(0.6, 0.9, nl.z);
  float front = smoothstep(0.6, 0.9, -nl.z);
  float upright = 1.0 - smoothstep(0.3, 0.6, abs(nl.y));
  float fres = at_fresnel(n, view);

  vec3 col = at_hullBase(n, view);
  vec3 em = vec3(0.0);

  if (type < 0.5) {
    // ================= freighter
    if (vPart < 0.5) {
      // Rows of portholes along the hull, a neon gunwale, running strips under the keel.
      float zone = at_box(L.zy, vec2(6.0, -5.2), vec2(84.0, 5.4), fwm) * step(L.y, -3.0);
      float lit = at_windows(vec2(L.z, L.y + 11.0), vec2(3.4, 3.6), vec2(0.5, 0.3), seed, 0.6, fl.zy);
      vec3 pane = mix(warm, sc_neon(1.0), step(0.85, sc_hash11(floor(L.z / 3.4) + seed)));
      em += pane * 1.25 * lit * zone * side;
      em += sc_neon(0.0) * 2.6 * sc_line(L.y - 3.6, 0.28, fl.y) * upright * (1.0 - front) * (1.0 - rear);
      em += sc_neon(1.0) * 1.2 * sc_line(L.y + 2.0, 0.14, fl.y) * side;
      float dash = sc_pulse(L.z / 9.0 - uTime * 0.3, 0.55, fl.z / 9.0);
      em += sc_neon(1.0) * 2.0 * sc_line(abs(L.x) - 7.5, 0.4, fl.x) * dash * under;
      em += sc_neon(2.0) * 0.22 * under;
      em += sc_neon(4.0) * 1.2 * sc_line(abs(L.x) - 16.6, 0.22, fl.x) * top;
      em += sc_neon(4.0) * 0.8 * sc_pulse(L.z / 17.5 + 0.02, 0.05, fl.z / 17.5) * top * step(abs(L.x), 16.6);
    } else if (vPart > 2.5 && vPart < 3.5) {
      // Containers: each stack its own colour, with lit bands, ribs and corner posts.
      float zi = (L.z + 64.0) / 17.5;
      float block = floor(zi);
      float zb = (zi - block) * 17.5;
      vec3 tint = sc_neon(floor(sc_hash11(block * 7.0 + seed) * 6.0));
      vec3 tint2 = sc_neon(floor(sc_hash11(block * 7.0 + seed + 3.0) * 6.0));
      float ribs = sc_pulse(zb / 2.0, 0.12, fl.z / 2.0);
      col = col * (1.0 - 0.5 * ribs * side) + tint * 0.035;
      float posts = sc_line(min(zb, 16.0 - zb) - 0.35, 0.2, fl.z);
      em += tint * 1.9 * posts * upright;
      em += tint * 0.8 * at_box(vec2(L.y, 0.0), vec2(9.2, 0.0), vec2(1.3, 1.0), fl.y) * side * (1.0 - ribs);
      em += tint2 * 1.6 * sc_line(L.y - 6.2, 0.16, fl.y) * upright;
      em += tint * 1.5 * sc_line(abs(L.x) - 12.5, 0.25, fl.x) * top;
      em += tint2 * 1.2 * sc_line(abs(zb - 8.0) - 6.6, 0.25, fl.z) * top * step(abs(L.x), 12.5);
    } else if (vPart > 0.5 && vPart < 1.5) {
      // Superstructure: a wall of lit windows, and the bridge's strip of glass on top.
      float bridge = step(22.0, L.y);
      vec2 uv = vec2(mix(L.x, L.z, side), L.y);
      vec2 fw2 = vec2(mix(fl.x, fl.z, side), fl.y);
      float lit = at_windows(uv + vec2(0.0, -5.4), vec2(2.4, 2.75), vec2(0.6, 0.45), seed + 11.0, 0.72, fw2);
      em += mix(warm, white, 0.3) * 1.4 * lit * upright * (1.0 - bridge);
      float glass = at_box(vec2(L.y, 0.0), vec2(24.9, 0.0), vec2(1.25, 1.0), fl.y) * sc_pulse(uv.x / 2.2, 0.9, fw2.x / 2.2);
      em += sc_neon(1.0) * 2.6 * glass * upright * bridge;
      em += sc_neon(0.0) * 2.0 * sc_line(L.y - 27.2, 0.14, fl.y) * upright * bridge;
      em += sc_neon(1.0) * 1.2 * sc_line(length(L.xz - vec2(0.0, 53.5)) - 5.0, 0.3, fwm) * top * bridge;
    } else if (vPart > 1.5 && vPart < 2.5) {
      vec2 q = vec2(abs(L.x) - 23.0, L.y + 5.0) / 3.6;
      float g = 1.0 - smoothstep(0.25, 1.0, length(q));
      em += (sc_neon(3.0) * 6.5 + white * 2.5 * g) * g * rear;
      em += sc_neon(3.0) * 1.8 * sc_pulse(L.z / 7.0, 0.12, fl.z / 7.0) * (1.0 - rear);
    } else {
      em += sc_neon(6.0) * 2.0 * sc_pulse(L.y / 4.0, 0.2, fl.y / 4.0) * step(20.0, L.y);
      em += sc_neon(1.0) * 1.6 * under * step(L.y, -10.0);
    }
  } else if (type < 1.5) {
    // ================= airship
    if (vPart < 0.5) {
      float sgn = L.x < 0.0 ? -1.0 : 1.0;
      vec2 pp = vec2(L.z * sgn, L.y);
      float inPanel = at_box(pp, vec2(0.0, 2.0), vec2(50.0, 10.5), fwm) * side;
      // LED cells: the picture is sampled at cell centres, and each cell is a round dot up close.
      float cell = 0.75;
      vec2 id = floor(pp / cell);
      vec2 cf = fract(pp / cell) - 0.5;
      float dotMask = 1.0 - smoothstep(0.3, 0.46, length(cf));
      float detail = sc_detail(cell, fwm);
      vec3 picture = at_screen((id + 0.5) * cell, vRun.z);
      em += picture * mix(0.55, dotMask * 1.25, detail) * inPanel;
      em += sc_neon(1.0) * 2.4 * sc_line(max(abs(pp.x) - 50.6, abs(pp.y - 2.0) - 11.1), 0.22, fwm) * side;
      // Envelope: station rings, a glowing nose, and the belly.
      float ringLine = sc_line(abs(L.z) - 66.0, 0.3, fl.z) + sc_line(abs(L.z) - 80.0, 0.25, fl.z);
      em += sc_neon(2.0) * 1.8 * ringLine;
      em += sc_neon(1.0) * 1.5 * (1.0 - smoothstep(-90.0, -84.5, L.z));
      em += sc_neon(0.0) * 1.6 * sc_line(L.y - 16.5, 0.22, fl.y) * step(abs(L.z), 62.0) * (1.0 - top);
      float belly = under * (1.0 - smoothstep(48.0, 64.0, abs(L.z + 4.0)));
      vec3 bellyCol = mix(sc_neon(0.0), sc_neon(1.0), 0.5 + 0.5 * sin(L.z * 0.045 + uTime * 0.4));
      float chase = sc_pulse(L.z / 7.0 - uTime * 0.22, 0.5, fl.z / 7.0);
      em += bellyCol * (0.5 + 0.35 * uBeat.x + 0.9 * chase * sc_line(abs(L.x) - 9.0, 1.1, fl.x)) * belly;
    } else if (vPart > 0.5 && vPart < 1.5) {
      vec2 uv = vec2(mix(L.x, L.z, side), L.y);
      vec2 fw2 = vec2(mix(fl.x, fl.z, side), fl.y);
      float glass = at_box(vec2(L.y, 0.0), vec2(-27.2, 0.0), vec2(1.7, 1.0), fl.y) * sc_pulse(uv.x / 2.4, 0.84, fw2.x / 2.4);
      em += mix(warm, white, 0.25) * 1.5 * glass * upright;
      // Underside: a lit landing grid rather than a slab of light.
      float grid = max(sc_line(abs(L.x) - 3.2, 0.25, fl.x), sc_pulse((L.z + 28.0) / 6.0, 0.1, fl.z / 6.0));
      em += sc_neon(4.0) * (0.12 + 1.5 * grid) * under;
    } else if (vPart > 1.5 && vPart < 2.5) {
      vec2 q = vec2(abs(L.x) - 28.25, L.y + 9.0) / 2.9;
      float g = 1.0 - smoothstep(0.25, 1.0, length(q));
      em += (sc_neon(2.0) * 6.0 + white * 2.0 * g) * g * rear;
      em += sc_neon(2.0) * 1.6 * sc_line(L.z - 24.0, 0.25, fl.z) * (1.0 - rear);
    } else {
      // Fins: chevron stripes and a lit trailing edge.
      float stripes = sc_pulse((L.z - abs(L.x + L.y) * 0.5) / 6.0, 0.3, (fl.z + 0.5 * (fl.x + fl.y)) / 6.0);
      em += sc_neon(0.0) * 0.9 * stripes * step(64.0, L.z);
      em += sc_neon(6.0) * 2.6 * smoothstep(89.0, 89.8, L.z);
    }
  } else {
    // ================= patrol craft
    float ph = fract(uTime * 1.5 + vRun.z);
    float flashA = mix(0.5, 0.06 + at_flash(ph), uMotion);
    float flashB = mix(0.5, 0.06 + at_flash(fract(ph + 0.5)), uMotion);
    vec3 blue = at_palette(1.62);
    if (vPart < 0.5) {
      // Livery: a chequered band along each flank, hood lines, hover pads.
      float band = at_box(vec2(L.y, 0.0), vec2(-1.05, 0.0), vec2(0.55, 1.0), fl.y) * step(-24.0, L.z);
      float chk = sc_pulse(L.z / 5.0, 0.5, fl.z / 5.0);
      em += mix(blue * 0.9, white * 0.75, chk) * band * side;
      em += blue * 2.2 * sc_line(L.y + 0.28, 0.07, fl.y) * side;
      em += sc_neon(1.0) * 1.6 * sc_line(abs(L.x) - 1.5, 0.1, fl.x) * top * step(-3.0, L.z);
      em += blue * 1.9 * at_box(vec2(abs(L.x), L.z), vec2(3.3, 9.0), vec2(1.0, 11.0), fwm) * under;
      em += white * 0.6 * at_box(L.xz, vec2(0.0, -20.0), vec2(1.0, 4.0), fwm) * under;
    } else if (vPart < 1.5) {
      col = vec3(0.005, 0.008, 0.02) + fres * uSkyHorizon * 0.25;
      em += sc_neon(1.0) * (0.2 + 0.5 * smoothstep(1.5, 3.6, L.y));
    } else if (vPart < 2.5) {
      vec2 q = vec2(abs(L.x) - 3.5, L.y) / vec2(1.3, 1.2);
      float g = 1.0 - smoothstep(0.3, 1.0, length(q));
      em += (sc_neon(1.0) * 6.0 + white * 2.0 * g) * g * rear;
    } else if (vPart < 4.5) {
      // Wings: a lit leading strip near the tip; fins: one red, one blue.
      float fin = step(1.0, L.y);
      em += sc_neon(1.0) * 1.8 * sc_line(abs(L.x) - 11.9, 0.14, fl.x) * (1.0 - fin) * (1.0 - smoothstep(0.3, 0.6, abs(nl.x)));
      vec3 finCol = L.x < 0.0 ? sc_neon(6.0) * flashA : blue * flashB;
      em += finCol * 1.8 * smoothstep(6.6, 7.2, L.y) * fin;
      em += white * 0.5 * sc_line(L.y - 5.0, 0.12, fl.y) * fin;
    } else if (vPart < 5.5) {
      em += (L.x < 0.0 ? sc_neon(6.0) * flashA : blue * flashB) * 4.5;
    } else {
      em += white * 5.0 * under + sc_neon(1.0) * 0.8;
    }
  }
  col += em * uGlow;
  gl_FragColor = vec4(sc_fog(col, vP), 1.0);
}
