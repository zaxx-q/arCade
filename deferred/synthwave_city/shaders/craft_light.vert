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

#define AT_SPRITE 0.0
#define AT_RIBBON 1.0
#define AT_CONE 2.0
#define AT_POOL 3.0

float at_spriteSize(vec3 centre, float size, float minPixels, out float coreRadius, out float gain) {
  float mpp = at_metresPerPixel(centre);
  float drawn = max(size, minPixels * mpp);
  float pixels = drawn / mpp;
  // The hot core is 0.16 of the sprite, but never under about a pixel; widening it dims it, and a
  // sprite held at its minimum size dims with the square root of how much it was enlarged, so far
  // craft stay visible as points without the far field turning into a wall of light.
  coreRadius = max(0.16, 0.85 / pixels);
  gain = (0.16 / coreRadius) * (0.16 / coreRadius) * sqrt(size / drawn);
  return drawn;
}

attribute vec3 aVert;   // sprite: corner x, corner y, light index. trail: side, along, 5.
uniform vec4 uLights[15];   // per shape and light: hull-local position, half-size
uniform vec4 uTrail[3];                      // per shape: y, z, half separation, line half-width
centroid varying vec2 vUv;
centroid varying vec3 vColor;
centroid varying vec4 vShape;
flat varying float vKind;
void main() {
  AtCraft c = at_craft();
  vec3 cam = vec3(0.0, cameraPosition.y, 0.0);
  int shape = int(aLook.y + 0.5);
  float scale = aBob.w;
  float seed = aLook.x;
  vec3 engine = sc_neon(aLook.z);
  vUv = aVert.xy;
  if (c.fade <= 0.0005) {
    vKind = AT_SPRITE; vColor = vec3(0.0); vShape = vec4(1.0);
    gl_Position = AT_NOWHERE;
    return;
  }
  if (aVert.z < 5.0 - 0.5) {
    int k = int(aVert.z + 0.5);
    vec4 light = uLights[shape * 5 + k];
    vec3 centre = sc_place(c.anchor, at_world(c, light.xyz * scale));
    float ahead = dot(normalize(cam - centre), c.fwd);   // 1 seen from dead ahead, -1 from behind
    vec3 tint;
    float level;
    float streak = 0.0;
    float aspect = 1.0;
    if (k < 2) {
      tint = engine;
      level = mix(0.1, 1.0, smoothstep(-0.25, 0.5, -ahead));
      level *= 0.88 + 0.12 * sin(uTime * 37.0 + seed * 420.0 + float(k) * 2.0);
      streak = 0.35;
    } else if (k == 2) {
      tint = mix(sc_neon(7.0), sc_neon(fract(seed * 13.0) < 0.5 ? 4.0 : 1.0), 0.45 * step(0.4, fract(seed * 29.0)));
      level = mix(0.05, 0.6, smoothstep(-0.1, 0.6, ahead)) + 0.4 * pow(max(ahead, 0.0), 14.0);
      streak = 1.0;
      aspect = 2.4;
    } else if (k == 3) {
      tint = sc_neon(7.0);
      float ph = fract(uTime * (0.7 + 0.5 * fract(seed * 5.0)) + seed * 31.0);
      float flash = step(ph, 0.06) + step(0.14, ph) * step(ph, 0.2);
      level = mix(0.3, 0.05 + 1.5 * flash, uMotion);
      streak = 0.6;
    } else {
      tint = sc_neon(6.0);
      level = 0.25 + 0.6 * (0.5 + 0.5 * sin(SC_TAU * (uTime * 0.55 + seed * 17.0)));
    }
    float size = light.w * scale;
    float coreRadius, gain;
    float drawn = at_spriteSize(centre, size, 2.2, coreRadius, gain);
    vec3 p = sc_sprite(centre, aVert.xy * vec2(aspect, 1.0), drawn);
    vKind = AT_SPRITE;
    vShape = vec4(coreRadius, streak, aspect, gain);
    vColor = tint * (level * c.fade * (1.0 - sc_fogAmount(centre)) * uGlow * mix(sqrt(size / drawn), 1.0, 0.5));
    gl_Position = light.w > 0.0 ? sc_project(p) : AT_NOWHERE;
  } else {
    // Trail: where the craft was, drawn back along the lane; it follows the weave and the bob.
    vec4 trail = uTrail[shape];
    float along = aVert.y;
    float len = aBob.z;
    float alongZ = aLane.z;
    vec3 lane = vec3(1.0 - alongZ, 0.0, alongZ);
    vec3 crs = vec3(alongZ, 0.0, 1.0 - alongZ);
    vec2 past = at_sway(uLoop - along * len / (c.speed * SC_LOOP_SECONDS)) - at_sway(uLoop);
    vec3 local = at_world(c, vec3(0.0, trail.x, trail.y) * scale)
      + crs * past.x + vec3(0.0, past.y, 0.0) - lane * (aLane.w * along * len);
    vec3 pt = sc_place(c.anchor, local);
    vec3 axis = lane * aLane.w;
    // The two exhaust lines spread apart only as far as the engines are seen side by side.
    vec3 across = cross(axis, cam - pt);
    float spread = abs(dot(across / max(length(across), 1e-4), c.right));
    float sep = trail.z * scale * spread;
    float lineW = trail.w * scale;
    float taper = mix(1.0, 0.45, along);
    float halfW = (sep + lineW * 2.6) * taper;
    float mpp = at_metresPerPixel(pt);
    float drawn = max(halfW, 1.8 * mpp);
    float sigma = max(lineW * taper / drawn, 0.42 * step(halfW, 1.8 * mpp));
    vec3 p = sc_axisBillboard(pt, axis, aVert.x, drawn);
    vKind = AT_RIBBON;
    vShape = vec4(sep * taper / drawn, sigma, seed, len);
    // Wider on screen than it really is: dim it by the same factor.
    float energy = min(max(lineW * taper / (sigma * drawn), sqrt(halfW / drawn)), 1.0);
    // A thin line far away reads dimmer than the same light close by: lift it a little with distance.
    energy *= 1.0 + 0.6 * smoothstep(250.0, 1100.0, length(pt - cam));
    vColor = engine * (2.2 * energy * c.fade * sc_axisFacing(pt, axis) * (1.0 - sc_fogAmount(pt)) * uGlow);
    gl_Position = sc_project(p);
  }
}
