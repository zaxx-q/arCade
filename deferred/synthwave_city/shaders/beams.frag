// Shader text taken from the web version (src/layers/beams/), adapted to GLSL 3.30 only by the prelude files.

#define BM_SEARCHLIGHT 0.0
#define BM_PILLAR 1.0
#define BM_LASER 2.0
#define BM_PILLAR_HALO 8.0
#define BM_LASER_GLOW 4.0

varying vec3 vP;
varying vec4 vCoord;
flat varying vec4 vShape;
flat varying vec4 vLook;

// Rare bright motes drifting along the beam: a few cells of a grid in beam space hold a twinkling
// dot. They fade out before a dot gets smaller than a couple of pixels, where they would only be
// grain.
float bm_motes(float s, float x, float seed, float cell, float lanes, float speed, float rarity, float fs) {
  vec2 g = vec2(s / cell - uTime * speed, (x * 0.5 + 0.5) * lanes);
  vec2 id = floor(g);
  vec2 f = fract(g) - 0.5;
  vec3 h = sc_hash33(vec3(id, seed));
  vec2 c = (h.yz - 0.5) * 0.5;
  float on = step(rarity, h.x);
  float tw = 0.5 + 0.5 * sin(uTime * (2.0 + 9.0 * h.y) + h.z * SC_TAU);
  vec2 q = f - c;
  return on * exp(-dot(q, q) * 70.0) * tw * tw * sc_detail(cell * 0.2, fs);
}

void main() {
  float len = vShape.x;
  float kind = vShape.w;
  // With multisampling a varying is extrapolated to sample points outside its triangle, and on a
  // ribbon seen nearly end-on that can be far outside: clamp, or r below goes negative and pow
  // returns NaN, which bloom then smears into a black block.
  float x = clamp(vCoord.x, -1.0, 1.0);
  float t = clamp(vCoord.y, 0.0, 1.0);
  float s = t * len;
  // Derivatives first, before anything branches on per-pixel data.
  float fs = fwidth(s);

  float seed = floor(vLook.y * 4096.0);
  vec3 tint = sc_neon(vLook.x);
  float edge = max(1.0 - x * x, 0.0);
  float r = mix(vShape.y, vShape.z, t);
  vec3 col;

  if (kind == BM_PILLAR) {
    // A column: a hot thread down the middle, a core about a radius wide, a glow a few radii wide
    // and a faint halo; bands of brightness rising slowly, a pulse climbing it once a bar, and a
    // swell on every beat. The thread keeps it reading as a shaft when the viewer is close.
    float u = x * BM_PILLAR_HALO;
    float core = exp(-u * u * 0.4);
    float hot = exp(-u * u * 6.0);
    float mid = exp(-u * u * 0.06);
    float halo = edge * edge * edge;
    float along = exp(-1.3 * t) * (1.0 - smoothstep(0.5, 1.0, t)) * smoothstep(0.0, 0.004, t);
    float bands = 0.74 + 0.26 * sin(SC_TAU * (s / 230.0 - uTime * 0.105));
    bands *= 0.86 + 0.14 * sin(SC_TAU * (s / 61.0 - uTime * 0.41 + fract(seed * 0.618)));
    bands = mix(0.74, bands, sc_detail(61.0, fs));
    // The second coordinate also creeps along the beam, so the lattice of the noise never lines up
    // with it and the haze does not read as evenly spaced beads.
    float haze = sc_fbm(vec2(s / 90.0 - uTime * 0.9, s / 370.0 + u * 0.3 + seed));
    float rays = sc_noise(vec2(u * 1.2 + seed * 0.61, uTime * 0.45 + s * 0.002));
    float shimmer = (0.45 + 1.1 * haze) * (0.75 + 0.5 * rays);
    float dPulse = (t - uBeat.y) * 14.0;
    float pulse = exp(-dPulse * dPulse) * 0.6 * uMotion;
    float beat = 1.0 + 0.3 * uBeat.x * uMotion;
    float motes = bm_motes(s, x, seed, 14.0, 9.0, 1.6, 0.95, fs) * edge;
    float body = (1.5 * hot + 1.5 * core + 0.6 * mid * edge + 0.1 * halo) * bands * shimmer * beat;
    body += (core + 0.3 * mid * edge) * pulse + motes * 2.2;
    col = mix(tint, vec3(1.0), 0.2 * core + 0.3 * hot) * body * along;
  } else if (kind == BM_LASER) {
    // A hair line: a narrow gaussian across the ribbon, nearly constant along it, with a faint
    // sparkle running outwards.
    float core = exp(-x * x * 5.5) * edge;
    float along = mix(1.0, 0.5, t) * (1.0 - smoothstep(0.6, 1.0, t)) * smoothstep(0.0, 0.002, t);
    float run = s / 3.0 - uTime * 26.0;
    float spark = sc_noise(vec2(run, seed));
    spark = spark * spark * spark * spark * 4.0 * sc_detail(3.0, fs);
    float flick = 1.0 - 0.18 * uMotion * sc_noise(vec2(uTime * 6.0, seed));
    float body = core * (1.5 + 1.6 * spark) * flick;
    col = mix(tint, vec3(1.0), 0.25 * core) * body * along;
  } else {
    // A searchlight: the chord through a soft cone, plus a tighter core. It thins out as it
    // widens. Long rays waver across it and haze drifts up it, which is the shimmer.
    float body = edge * sqrt(edge);
    float core = exp(-x * x * 9.0);
    float along = pow(vShape.y / r, 0.6) * exp(-1.9 * t) * (1.0 - smoothstep(0.55, 1.0, t)) * smoothstep(0.0, 0.003, t);
    float rays = 0.6 * sc_noise(vec2(x * 3.0 + seed, uTime * 0.35 + s * 0.0025))
               + 0.4 * sc_noise(vec2(x * 7.0 - seed, uTime * 0.8 - s * 0.006));
    float puffs = sc_fbm(vec2(s / 70.0 - uTime * 0.6, s / 290.0 + x * 0.9 + seed * 0.37));
    float shimmer = (0.35 + 1.3 * rays) * (0.15 + 1.7 * puffs);
    float motes = bm_motes(s, x, seed, 10.0, 4.0, 1.0, 0.9, fs) * edge;
    float light = (0.9 * body + 0.9 * core) * shimmer + motes * 6.0;
    col = mix(tint, vec3(1.0), 0.35 * core) * light * along;
  }

  // Close to the camera a ribbon is no longer a fair stand-in for a volume (and it twists hard to
  // keep facing the viewer), so a beam fades out as the viewer flies into it.
  float dist = length(vP - vec3(0.0, cameraPosition.y, 0.0));
  float nearFade = smoothstep(1.5 * r, 6.0 * r + 15.0, dist);

  col *= vLook.z * clamp(vCoord.z, 0.0, 1.0) * clamp(vCoord.w, 0.0, 1.5) * nearFade * uGlow;
  col *= 1.0 - sc_fogAmount(vP);
  gl_FragColor = vec4(col, 1.0);
}
