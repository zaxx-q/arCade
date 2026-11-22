// Shader text taken from the web version (src/layers/airtraffic/), adapted to GLSL 3.30 only by the prelude files.

// fract(n * loop) for a whole number n, keeping the low bits of loop that n * loop would round away.
float at_laps(float n, float loop) {
  float hi = floor(loop * 4096.0) / 4096.0;
  return fract(n * hi) + n * (loop - hi);
}

attribute vec4 aRoute;  // fixed coordinate, altitude, axis, direction
attribute vec4 aRun;    // laps, phase, seed, type
uniform vec2 uHeroBob[3];   // per type: vertical drift amplitude, whole cycles per loop

struct AtHero {
  vec3 anchor;
  vec3 right;
  vec3 fwd;
  float fade;
};

AtHero at_hero() {
  AtHero h;
  float alongZ = aRoute.z;
  vec3 lane = vec3(1.0 - alongZ, 0.0, alongZ);
  vec3 crs = vec3(alongZ, 0.0, 1.0 - alongZ);
  float s = fract(aRoute.w * at_laps(aRun.x, uLoop) + aRun.y) * SC_TILE;
  vec2 bob = uHeroBob[int(aRun.w + 0.5)];
  float y = aRoute.y + bob.x * sin(SC_TAU * (at_laps(bob.y, uLoop) + aRun.z));
  h.anchor = lane * s + crs * aRoute.x + vec3(0.0, y, 0.0);
  h.fwd = lane * aRoute.w;
  h.right = cross(h.fwd, vec3(0.0, 1.0, 0.0));
  h.fade = sc_farFade(sc_wrap(h.anchor.xz));
  return h;
}
vec3 at_world(AtHero h, vec3 local) { return h.right * local.x + vec3(0.0, local.y, 0.0) - h.fwd * local.z; }

// The patrol craft's searchlight: a slow swing ahead and astern, a smaller one side to side.
vec3 at_searchDir(AtHero h, float seed) {
  float a = 0.2 + 0.36 * sin(SC_TAU * (at_laps(372.0, uLoop) + seed));
  float b = 0.035 * sin(SC_TAU * (at_laps(585.0, uLoop) + seed * 3.0));
  return normalize(vec3(0.0, -1.0, 0.0) + h.fwd * a + h.right * b);
}

const vec4 AT_NOWHERE = vec4(0.0, 0.0, 2.0, 1.0);   // outside the clip volume: the primitive is dropped
float at_metresPerPixel(vec3 p) {
  float dist = length(p - vec3(0.0, cameraPosition.y, 0.0));
  return 2.0 * dist / (uResolution.y * projectionMatrix[1][1]);
}
// Palette colour for an index that may be fractional: 1.6 is a blue between cyan and violet.
vec3 at_palette(float index) {
  float i = floor(index);
  return mix(sc_neon(i), sc_neon(i + 1.0), index - i);
}

attribute float aPart;
attribute float aType;
varying vec3 vP;
varying vec3 vLocal;
flat varying float vPart;
flat varying vec4 vRun;
flat varying vec3 vRight;
flat varying vec3 vFwd;
void main() {
  AtHero h = at_hero();
  vec3 p = sc_place(h.anchor, at_world(h, position));
  vP = p;
  vLocal = position;
  vPart = aPart;
  vRun = aRun;
  vRight = h.right;
  vFwd = h.fwd;
  gl_Position = abs(aType - aRun.w) < 0.5 && h.fade > 0.0005 ? sc_project(p) : AT_NOWHERE;
}
