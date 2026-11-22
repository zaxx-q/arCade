// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/holograms/), adapted to GLSL 3.30 only by the prelude files.


attribute vec3 aPos;
attribute vec4 aSize;   // width, height, yaw, seed
attribute vec4 aLook;   // ink A, ink B, wipe cycles per loop, wipe style
attribute vec2 aFit;    // panel uv -> ad uv scale
attribute vec4 aRectA;
attribute vec4 aRectB;
varying vec2 vUv;
varying vec3 vP;
flat varying vec4 vRectA;
flat varying vec4 vRectB;
flat varying vec4 vLook;   // ink A, ink B, share of the panel showing the second ad, wipe style
flat varying vec4 vDim;    // width, height, quantised seed, part
flat varying vec4 vFit;    // fit x, fit y, brightness of this side, projector half-width
flat varying vec3 vNormal;

void main() {
  float part = position.z;
  float w = aSize.x, h = aSize.y;
  float ph = aSize.w;
  // Bob and sway: whole cycles per loop, so the motion closes exactly when time wraps.
  float bob = sin(SC_TAU * fract(uLoop * 585.0 + ph)) * (0.7 + 0.7 * fract(ph * 7.0)) * uMotion;
  float sway = 0.035 * sin(SC_TAU * fract(uLoop * 372.0 + ph * 3.0)) * uMotion;
  float lean = 0.022 * sin(SC_TAU * fract(uLoop * 455.0 + ph * 5.0)) * uMotion;
  float a = aSize.z + sway;
  vec3 n = vec3(-sin(a), 0.0, -cos(a));
  vec3 r = vec3(-cos(a), 0.0, sin(a));
  vec3 up = normalize(vec3(0.0, 1.0, 0.0) + n * lean);
  // The image is flipped for a viewer behind the panel, so the ad reads from both sides.
  vec2 d = sc_wrap(aPos.xz);
  float side = dot(vec2(-d.x, -d.y), n.xz) >= 0.0 ? 1.0 : -1.0;

  float gap = clamp(h * 0.2, 3.0, 9.0);
  float lampHalf = 0.5 * clamp(w * 0.16, 2.5, 12.0);
  vec3 foot = vec3(0.0, -0.5 * h - gap, 0.0);
  vec3 local;
  if (part < 0.5) {
    local = r * (position.x * w) + up * (position.y * h) + vec3(0.0, bob, 0.0);
    vUv = vec2(0.5 + position.x * side, 0.5 + position.y);
  } else if (part < 1.5) {
    float t = position.y + 0.5;
    float across = position.x * 2.0 * mix(lampHalf, 0.5 * w, t);
    local = r * across + mix(foot, up * (-0.5 * h) + vec3(0.0, bob, 0.0), t);
    vUv = vec2(across, t);
  } else {
    local = r * (position.x * lampHalf * 3.2) + foot + vec3(0.0, position.y * 2.6, 0.0);
    vUv = position.xy * 2.0;
  }
  vec3 p = sc_place(aPos, local);
  vP = p;

  float k = 0.0;
  if (aLook.z > 0.5) {
    float cycle = fract(uLoop * aLook.z + ph);
    float wipe = 1.1 * aLook.z / SC_LOOP_SECONDS;
    k = cycle < 0.5 ? 1.0 - clamp(cycle / wipe, 0.0, 1.0) : clamp((cycle - 0.5) / wipe, 0.0, 1.0);
  }
  vRectA = aRectA;
  vRectB = aRectB;
  vLook = vec4(aLook.x, aLook.y, k, aLook.w);
  vDim = vec4(w, h, floor(ph * 4096.0), part);
  vFit = vec4(aFit, side > 0.0 ? 1.0 : 0.82, lampHalf);
  vNormal = n;
  gl_Position = sc_project(p);
}
