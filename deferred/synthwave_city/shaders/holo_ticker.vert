// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/holograms/), adapted to GLSL 3.30 only by the prelude files.


attribute vec3 aPos;
attribute vec4 aExt;    // half-extent x, half-extent z, band height, seed
attribute vec4 aLook;   // ink A, ink B, strip repeats round the ring, laps per loop
attribute vec4 aRect;
varying vec3 vP;
varying vec3 vBand;     // strip coordinate (in strips), metres round the ring, 0..1 up the band (-0.5..1.5 over the quad)
flat varying vec4 vRect;
flat varying vec4 vLook;   // ink A, ink B, quantised seed, band height
flat varying vec3 vNormal;

void main() {
  float side = position.x, t = position.y;
  float w = aExt.x, d = aExt.y, h = aExt.z;
  // Walk the ring so that the text runs left to right for a viewer outside it.
  vec2 xz;
  vec2 nrm;
  float s0;
  float len;
  if (side < 0.5) { xz = vec2(mix(w, -w, t), -d); nrm = vec2(0.0, -1.0); s0 = 0.0; len = 2.0 * w; }
  else if (side < 1.5) { xz = vec2(-w, mix(-d, d, t)); nrm = vec2(-1.0, 0.0); s0 = 2.0 * w; len = 2.0 * d; }
  else if (side < 2.5) { xz = vec2(mix(-w, w, t), d); nrm = vec2(0.0, 1.0); s0 = 2.0 * w + 2.0 * d; len = 2.0 * w; }
  else { xz = vec2(w, mix(d, -d, t)); nrm = vec2(1.0, 0.0); s0 = 4.0 * w + 2.0 * d; len = 2.0 * d; }
  float perimeter = 4.0 * (w + d);
  float metres = s0 + t * len;
  // Whole laps per loop: the scroll closes exactly when time wraps (architecture.md 4.3).
  float scroll = fract(uLoop * aLook.w + aExt.w);
  vec3 p = sc_place(aPos, vec3(xz.x, (position.z - 0.5) * 2.0 * h, xz.y));
  vP = p;
  vBand = vec3((metres / perimeter + scroll) * aLook.z, metres, position.z * 2.0 - 0.5);
  vRect = aRect;
  vLook = vec4(aLook.x, aLook.y, floor(aExt.w * 4096.0), h);
  vNormal = vec3(nrm.x, 0.0, nrm.y);
  gl_Position = sc_project(p);
}
