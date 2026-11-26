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
flat varying vec4 vLook;
flat varying vec3 vRight;
flat varying vec3 vUp;
flat varying vec3 vFwd;
void main() {
  // Derivatives first, before anything branches.
  vec3 dx = dFdx(vP), dy = dFdy(vP);
  vec3 fl = fwidth(vLocal);
  vec3 L = vLocal;
  vec2 rq = vec2((abs(L.x) - 1.1) / 0.6, (L.z - 0.05) / 0.8);
  float rotor = length(rq);
  float fwRotor = fwidth(rotor);
  float fwm = max(fl.x, max(fl.y, fl.z));

  vec3 view = normalize(vP - vec3(0.0, cameraPosition.y, 0.0));
  vec3 n = at_normal(dx, dy, view);
  vec3 nl = vec3(dot(n, vRight), dot(n, vUp), -dot(n, vFwd));

  float shape = vLook.y;
  vec3 engine = sc_neon(vLook.z);
  vec3 accent = sc_neon(vLook.w);
  vec3 white = sc_neon(7.0);
  float side = smoothstep(0.3, 0.65, abs(nl.x));
  float top = smoothstep(0.5, 0.8, nl.y);
  float under = smoothstep(0.45, 0.75, -nl.y);
  float rear = smoothstep(0.6, 0.9, nl.z);
  float front = smoothstep(0.25, 0.6, -nl.z);

  vec3 col = at_hullBase(n, view);
  vec3 em = vec3(0.0);
  float fres = at_fresnel(n, view);

  if (shape < 0.5) {
    // ---- wedge car
    if (vPart < 0.5) {
      float belt = mix(-0.27, -0.12, smoothstep(-4.0, 3.3, L.z));
      em += accent * 2.4 * sc_line(L.y - belt, 0.04, fl.y) * side * smoothstep(-3.9, -3.2, L.z);
      float pads = at_box(vec2(abs(L.x), L.z), vec2(0.62, 0.7), vec2(0.2, 1.7), fwm);
      em += accent * (0.7 + 1.3 * sc_pulse(L.z / 0.55 + uTime * 2.2, 0.5, fl.z / 0.55)) * pads * under;
      em += (engine * 4.5 + white * 1.2) * at_box(vec2(abs(L.x), L.y), vec2(0.8, 0.0), vec2(0.42, 0.15), fwm) * rear;
      em += accent * 1.3 * sc_line(L.x, 0.025, fl.x) * top * (1.0 - smoothstep(-2.2, -1.9, L.z));
    } else if (vPart < 1.5) {
      // Canopy: dark glass with a glow inside, a bright roof rail and a centre spar.
      col = vec3(0.004, 0.006, 0.016) + fres * uSkyHorizon * 0.4;
      float spar = sc_line(L.x, 0.045, fl.x);
      em += mix(accent, white, 0.2) * (0.05 + 0.2 * smoothstep(-1.9, 1.1, L.z)) * (1.0 - spar);
      em += accent * 1.8 * sc_line(L.z - 1.02, 0.05, fl.z) * (1.0 - rear);
    } else {
      // Fins: a lit trailing edge.
      em += engine * 1.6 * smoothstep(3.5, 3.58, L.z);
    }
  } else if (shape < 1.5) {
    // ---- hauler
    if (vPart < 0.5) {
      vec3 glass = mix(white, sc_neon(4.0), 0.35);
      float pane = 0.2 + 0.5 * (1.0 - smoothstep(0.4, 1.3, L.y));
      em += glass * pane * at_box(L.xy, vec2(0.0, 0.85), vec2(1.45, 0.42), fwm) * front * step(L.z, -6.6)
        * (1.0 - 0.8 * sc_line(L.x, 0.05, fl.x));
      em += glass * pane * at_box(L.zy, vec2(-5.7, 0.85), vec2(0.7, 0.4), fwm) * side;
      em += accent * 2.2 * sc_line(L.y + 0.95, 0.05, fl.y) * side;
      em += accent * 1.5 * at_box(vec2(abs(L.x), L.z), vec2(1.0, -6.6), vec2(0.35, 1.5), fwm) * under;
    } else if (vPart > 2.5) {
      float ribs = sc_pulse(L.z / 1.31, 0.1, fl.z / 1.31);
      col *= 1.0 - 0.55 * ribs * side;
      col += accent * 0.02;
      // A lit band of colour down each flank, broken into panels.
      float band = at_box(vec2(L.y, 0.0), vec2(0.45, 0.0), vec2(0.3, 1.0), fl.y);
      float slats = 0.55 + 0.45 * sc_pulse(L.z / 0.36, 0.6, fl.z / 0.36);
      em += accent * 0.7 * slats * band * side * sc_pulse(L.z / 4.36 + 0.08, 0.86, fl.z / 4.36);
      // Marker lights along the top and bottom rails.
      vec2 mc = vec2((fract(L.z / 2.18) - 0.5) * 2.18, abs(L.y - 0.2) - 1.72);
      float dots = 1.0 - smoothstep(0.08, 0.08 + max(fwm, 0.01), length(mc));
      float rail = sc_line(abs(L.y - 0.2) - 1.72, 0.05, fl.y) * 0.3;
      em += sc_neon(4.0) * 3.2 * mix(rail, dots, sc_detail(2.18, fl.z)) * side;
      em += accent * 1.3 * sc_line(abs(L.x) - 1.7, 0.07, fl.x) * top;
      em += sc_neon(6.0) * 4.0 * at_box(vec2(abs(L.x), L.y), vec2(1.85, -1.3), vec2(0.28, 0.16), fwm) * rear;
      em += accent * 1.6 * sc_line(L.y - 1.85, 0.06, fl.y) * rear;
      float pads = at_box(vec2(abs(L.x), L.z), vec2(1.2, 1.8), vec2(0.45, 5.2), fwm);
      em += accent * (0.6 + 1.2 * sc_pulse(L.z / 0.9 + uTime * 1.6, 0.5, fl.z / 0.9)) * pads * under;
    } else {
      em += (engine * 5.0 + white * 1.5) * at_box(vec2(abs(L.x), L.y), vec2(2.95, 0.0), vec2(0.42, 0.52), fwm) * rear;
      em += engine * 2.0 * sc_line(L.z - 8.5, 0.07, fl.z) * (1.0 - rear);
    }
  } else {
    // ---- drone
    if (vPart < 0.5) {
      em += accent * 2.4 * (1.0 - smoothstep(-1.45, -1.0, L.z));
      em += engine * 1.6 * sc_line(L.z - 0.3, 0.05, fl.z);
    } else {
      float flat_ = smoothstep(0.4, 0.7, abs(nl.y));
      em += accent * 2.4 * sc_line(rotor - 0.6, 0.1, fwRotor) * flat_;
      em += engine * 0.5 * (1.0 - smoothstep(0.0, 0.6, rotor)) * flat_;
    }
  }
  col += em * uGlow;
  gl_FragColor = vec4(sc_fog(col, vP), 1.0);
}
