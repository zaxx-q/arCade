// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/holograms/), adapted to GLSL 3.30 only by the prelude files.


attribute vec3 aBary;
attribute vec3 aAnchor;
attribute vec4 aSpin;   // spin laps, phase, tilt, precession laps
attribute vec4 aLook;   // palette index, brightness, kind, quantised seed
attribute vec4 aStyle;  // line half-width (m), face fill, radius, bob weight
varying vec3 vBary;
varying vec3 vP;
varying vec3 vLocal;
varying float vMpp;
flat varying vec4 vLook;
flat varying vec4 vStyle;

vec3 holo_rotY(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(c * p.x + s * p.z, p.y, c * p.z - s * p.x); }
vec3 holo_rotX(vec3 p, float a) { float c = cos(a), s = sin(a); return vec3(p.x, c * p.y - s * p.z, s * p.y + c * p.z); }

void main() {
  float kind = aLook.z;
  float ph = aSpin.y;
  vec3 anchor = aAnchor;
  anchor.y += aStyle.w * 1.6 * sin(SC_TAU * fract(uLoop * 512.0 + ph)) * uMotion;
  vec3 p;
  if (kind > 0.5 && kind < 1.5) {
    vec3 centre = sc_place(anchor, vec3(0.0));
    p = sc_sprite(centre, position.xy, position.z * sc_farFade(sc_wrap(anchor.xz)));
    vLocal = vec3(position.xy, 0.0);
  } else {
    vec3 q = holo_rotY(position, SC_TAU * fract(uLoop * aSpin.x + ph));
    q = holo_rotX(q, aSpin.z);
    q = holo_rotY(q, SC_TAU * fract(uLoop * aSpin.w + ph * 1.7));
    vLocal = q;
    p = sc_place(anchor, q);
  }
  vP = p;
  vBary = aBary;
  vLook = aLook;
  vStyle = aStyle;
  gl_Position = sc_project(p);
  vMpp = 2.0 * max(gl_Position.w, 0.1) / (uResolution.y * projectionMatrix[1][1]);
}
