// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/airtraffic/), adapted to GLSL 3.30 by the prelude files; the one other change is at_pull() below.

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

// The web version gives these lights a small polygon offset towards the camera (factor -1, units -2) so the
// pool of light does not fight the road's depth. rlgl has no polygon offset: the same pull in clip space.
vec4 at_pull(vec4 clip) {
  clip.z -= clip.w * 5.0e-7;
  return clip;
}

attribute vec3 aLocal;  // hull-local position
attribute vec4 aSpec;   // half-size (or plume length), palette index, mode, phase
attribute vec3 aQuad;   // corner x, corner y, type
centroid varying vec2 vUv;
centroid varying vec3 vColor;
centroid varying vec4 vShape;
flat varying float vKind;
float at_flash2(float ph) { return step(ph, 0.07) + step(0.15, ph) * step(ph, 0.22); }
void main() {
  AtHero h = at_hero();
  vec3 cam = vec3(0.0, cameraPosition.y, 0.0);
  float seed = aRun.z;
  float mode = aSpec.z;
  vec3 tint = at_palette(aSpec.y);
  vUv = aQuad.xy;
  vKind = AT_SPRITE;
  vShape = vec4(1.0);
  vColor = vec3(0.0);
  if (abs(aQuad.z - aRun.w) > 0.5 || h.fade <= 0.0005) {
    gl_Position = AT_NOWHERE;
    return;
  }
  vec3 origin = at_world(h, aLocal);
  if (mode < 9.5) {
    vec3 centre = sc_place(h.anchor, origin);
    float ahead = dot(normalize(cam - centre), h.fwd);
    float level = 3.0;
    float streak = 0.5;
    float aspect = 1.0;
    if (mode < 0.5) {
      level = 2.6;
    } else if (mode < 1.5) {
      float ph = fract(uTime * 0.8 + aSpec.w + seed);
      level = mix(1.5, 0.1 + 8.0 * at_flash2(ph), uMotion);
      streak = 1.0; aspect = 2.0;
    } else if (mode < 2.5) {
      level = 0.5 + 3.2 * pow(max(0.5 + 0.5 * sin(SC_TAU * (uTime * 0.45 - aSpec.w + seed)), 0.0), 2.0);
    } else if (mode < 4.5) {
      float ph = fract(uTime * 1.5 + seed + (mode > 3.5 ? 0.5 : 0.0));
      level = mix(2.0, 0.15 + 9.0 * at_flash2(ph), uMotion);
      streak = 1.0; aspect = 2.2;
    } else if (mode < 5.5) {
      level = 6.0 * mix(0.12, 1.0, smoothstep(-0.25, 0.5, -ahead));
      level *= 0.9 + 0.1 * sin(uTime * 23.0 + seed * 90.0 + aLocal.x);
    } else if (mode < 6.5) {
      vec3 dir = at_searchDir(h, seed);
      float into = max(dot(normalize(cam - centre), dir), 0.0);
      level = 3.0 + 9.0 * pow(into, 6.0);
      streak = 1.0; aspect = 2.4;
    } else {
      level = 4.0 * (mix(0.08, 0.8, smoothstep(-0.1, 0.6, ahead)) + 0.8 * pow(max(ahead, 0.0), 14.0));
      streak = 1.0; aspect = 2.4;
    }
    float coreRadius, gain;
    float drawn = at_spriteSize(centre, aSpec.x, 2.4, coreRadius, gain);
    vec3 p = sc_sprite(centre, aQuad.xy * vec2(aspect, 1.0), drawn);
    vShape = vec4(coreRadius, streak, aspect, gain);
    vColor = tint * (level / 6.0 * h.fade * (1.0 - sc_fogAmount(centre)) * uGlow);
    gl_Position = at_pull(sc_project(p));
  } else if (mode < 11.5) {
    // Searchlight: the cone from the lamp to the road, and the pool where it lands.
    vec3 dir = at_searchDir(h, seed);
    float reach = (h.anchor.y + origin.y) / max(-dir.y, 0.2);
    if (mode < 10.5) {
      float along = aQuad.y;
      vec3 pt = sc_place(h.anchor, origin + dir * (reach * along));
      float halfW = mix(1.1, 10.0, along);
      vec3 p = sc_axisBillboard(pt, dir, aQuad.x, halfW);
      vKind = AT_CONE;
      vShape = vec4(0.0, 0.0, seed, reach);
      vColor = mix(tint, sc_neon(1.0), 0.25) * (1.5 * h.fade * (1.0 - sc_fogAmount(pt)) * uGlow);
      gl_Position = at_pull(sc_project(p));
    } else {
      vec3 ground = h.anchor + origin + dir * reach;
      vec3 p = sc_place(vec3(ground.x, 0.0, ground.z), vec3(aQuad.x * 11.0, 0.5, aQuad.y * 11.0));
      vKind = AT_POOL;
      vColor = mix(tint, sc_neon(1.0), 0.25) * (1.3 * h.fade * (1.0 - sc_fogAmount(p)) * uGlow);
      gl_Position = at_pull(sc_project(p));
    }
  } else {
    // Exhaust plume, straight astern.
    float along = aQuad.y;
    float len = aSpec.x;
    vec3 pt = sc_place(h.anchor, origin - h.fwd * (len * along));
    float halfW = len * mix(0.07, 0.025, along);
    vec3 p = sc_axisBillboard(pt, h.fwd, aQuad.x, halfW);
    vKind = AT_RIBBON;
    vShape = vec4(0.0, 0.5, seed + aLocal.x * 0.01, len);
    vColor = tint * (1.1 * h.fade * sc_axisFacing(pt, h.fwd) * (1.0 - sc_fogAmount(pt)) * uGlow);
    gl_Position = at_pull(sc_project(p));
  }
}
