// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/airtraffic/), adapted to GLSL 3.30 only by the prelude files.

// fract(n * loop) for a whole number n, keeping the low bits of loop that n * loop would round away.
float at_laps(float n, float loop) {
  float hi = floor(loop * 4096.0) / 4096.0;
  return fract(n * hi) + n * (loop - hi);
}

attribute vec4 aLane;   // fixed coordinate, altitude, axis (0 along x, 1 along z), direction (+1 / -1)
attribute vec4 aMove;   // laps, phase, weave amplitude, weave cycles per loop
attribute vec4 aBob;    // bob amplitude, bob cycles per loop, trail length, scale
attribute vec4 aLook;   // seed, shape, engine palette index, accent palette index

struct AtCraft {
  vec3 anchor;   // tile coordinates of the craft's centre
  vec3 right;
  vec3 up;
  vec3 fwd;
  float speed;   // metres per second along the lane
  float fade;    // far fade of the anchor
};

// Sideways and vertical sway at a loop phase. Whole cycles per loop, so the loop seam is seamless.
vec2 at_sway(float loop) {
  return vec2(
    aMove.z * sin(SC_TAU * (at_laps(aMove.w, loop) + aLook.x)),
    aBob.x * sin(SC_TAU * (at_laps(aBob.y, loop) + aLook.x * 7.0)));
}

AtCraft at_craft() {
  AtCraft c;
  float alongZ = aLane.z;
  vec3 lane = vec3(1.0 - alongZ, 0.0, alongZ);
  vec3 crs = vec3(alongZ, 0.0, 1.0 - alongZ);
  float s = fract(aLane.w * at_laps(aMove.x, uLoop) + aMove.y) * SC_TILE;
  vec2 sway = at_sway(uLoop);
  c.anchor = lane * s + crs * (aLane.x + sway.x) + vec3(0.0, aLane.y + sway.y, 0.0);
  c.speed = aMove.x * (SC_TILE / SC_LOOP_SECONDS);

  // Heading follows the velocity; the craft banks into its weave.
  float wRate = SC_TAU * aMove.w / SC_LOOP_SECONDS;
  float wPhase = SC_TAU * (at_laps(aMove.w, uLoop) + aLook.x);
  float latVel = aMove.z * wRate * cos(wPhase);
  float latAcc = -aMove.z * wRate * wRate * sin(wPhase);
  float bRate = SC_TAU * aBob.y / SC_LOOP_SECONDS;
  float vertVel = aBob.x * bRate * cos(SC_TAU * (at_laps(aBob.y, uLoop) + aLook.x * 7.0));
  vec3 f = normalize(lane * (aLane.w * c.speed) + crs * latVel + vec3(0.0, vertVel, 0.0));
  vec3 r = normalize(cross(f, vec3(0.0, 1.0, 0.0)));
  vec3 u = cross(r, f);
  float roll = clamp(dot(crs, r) * latAcc * 0.4, -0.5, 0.5);
  float cr = cos(roll), sr = sin(roll);
  c.right = r * cr - u * sr;
  c.up = u * cr + r * sr;
  c.fwd = f;
  c.fade = sc_farFade(sc_wrap(c.anchor.xz));
  return c;
}

// Hull-local (x right, y up, z back) to an offset from the anchor.
vec3 at_world(AtCraft c, vec3 local) { return c.right * local.x + c.up * local.y - c.fwd * local.z; }

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
uniform float uHullFar;
varying vec3 vP;
varying vec3 vLocal;
flat varying float vPart;
flat varying vec4 vLook;
flat varying vec3 vRight;
flat varying vec3 vUp;
flat varying vec3 vFwd;
void main() {
  AtCraft c = at_craft();
  vec3 p = sc_place(c.anchor, at_world(c, position * aBob.w));
  vP = p;
  vLocal = position;
  vPart = aPart;
  vLook = aLook;
  vRight = c.right;
  vUp = c.up;
  vFwd = c.fwd;
  vec2 d = sc_wrap(c.anchor.xz);
  float dist = length(vec3(d.x, c.anchor.y - cameraPosition.y, d.y));
  gl_Position = dist < uHullFar && c.fade > 0.0005 ? sc_project(p) : AT_NOWHERE;
}
