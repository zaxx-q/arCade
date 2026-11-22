// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version, adapted to GLSL 3.30 only by the prelude files.

varying vec3 vDir;

#define SKY_SUN_R 0.1569181914556899
#define SKY_BASE -0.002
#define SKY_FOOT -0.012
#define SKY_MOUNTAIN_TOP 0.188
#define SKY_LIGHT_COLS 1500.0
#define SKY_LIGHT_ROWS 4.0
#define SKY_LIGHT_ROW_H 0.0034

// Value noise on a 3D lattice, 0..1. Sampled on the direction, so it has no seam on the dome.
float sky_noise3(vec3 p) {
  vec3 i = floor(p);
  vec3 u = fract(p);
  u = u * u * (3.0 - 2.0 * u);
  float n000 = sc_hash31(i);
  float n100 = sc_hash31(i + vec3(1.0, 0.0, 0.0));
  float n010 = sc_hash31(i + vec3(0.0, 1.0, 0.0));
  float n110 = sc_hash31(i + vec3(1.0, 1.0, 0.0));
  float n001 = sc_hash31(i + vec3(0.0, 0.0, 1.0));
  float n101 = sc_hash31(i + vec3(1.0, 0.0, 1.0));
  float n011 = sc_hash31(i + vec3(0.0, 1.0, 1.0));
  float n111 = sc_hash31(i + vec3(1.0, 1.0, 1.0));
  return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
             mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

// A line at distance dPx (pixels) with half-width wPx (pixels), antialiased over one pixel;
// thinner than a pixel it dims instead of aliasing.
float sky_linePx(float dPx, float wPx) { return sc_line(dPx, wPx, 1.0); }

// The cube face a direction falls on. q: the direction in that face's frame (xy across the face,
// z out of it and positive). On the four side faces q.y is world up, so sparkle arms stand upright.
float sky_face(vec3 d, out vec3 q) {
  vec3 a = abs(d);
  float face;
  if (a.x >= a.y && a.x >= a.z) { q = vec3(d.z, d.y, a.x); face = d.x > 0.0 ? 0.0 : 1.0; }
  else if (a.y >= a.z) { q = vec3(d.x, d.z, a.y); face = d.y > 0.0 ? 2.0 : 3.0; }
  else { q = vec3(d.x, d.y, a.z); face = d.z > 0.0 ? 4.0 : 5.0; }
  return face;
}

// One layer of stars: a grid of n x n cells on each cube face, at most one star per cell, kept
// clear of the cell edge so no star is cut. A star is a small Gaussian at least 0.8 pixel in
// radius, so it does not flicker as it crosses pixel centres when the camera turns.
// size: x smallest and y largest radius (radians), z brightest intensity.
vec3 sky_starLayer(vec3 q, vec2 w, float face, float n, float seed, float px, float density, float sparkleShare, vec3 size) {
  vec2 cell = floor(clamp((w * 0.5 + 0.5) * n, 0.0, n - 0.001));
  float key = face * 8.0 + seed;
  vec3 r = sc_hash33(vec3(cell, key));
  vec3 r2 = sc_hash33(vec3(cell, key + 0.5));
  vec2 cw = (cell + 0.16 + 0.68 * r2.xy) / n * 2.0 - 1.0;
  vec3 o = q - normalize(vec3(tan(cw * (SC_PI * 0.25)), 1.0));

  float sparkle = step(r2.z, sparkleShare);
  float b = r.y * r.y;
  b = mix(b * b, 0.85, sparkle);
  float rad = mix(size.x, size.y, b);
  float used = max(rad, 0.8 * px);
  float point = mix(0.07, 1.0, b) * size.z * (rad / used) * exp(-dot(o, o) / (used * used));
  float twinkle = 1.0 + (0.28 + 0.3 * sparkle) * uMotion * sin(uTime * (0.8 + 3.5 * r.z) + r.z * 97.0);

  // Four-point arms, for the few sparkles.
  float len = 0.021 * (0.55 + 0.45 * r.z);
  float aw = max(0.0003, 0.7 * px);
  vec2 ao = abs(o.xy);
  float ax = max(1.0 - ao.x / len, 0.0);
  float ay = max(1.0 - ao.y / len, 0.0);
  float arms = exp(-(ao.y * ao.y) / (aw * aw)) * ax * ax * ax + exp(-(ao.x * ao.x) / (aw * aw)) * ay * ay * ay;
  arms *= 1.5 * (0.0003 / aw + 0.35);

  vec3 tint = sc_neon(7.0);
  tint = mix(tint, sc_neon(1.0), 0.55 * smoothstep(0.72, 0.9, r.z));
  tint = mix(tint, sc_neon(4.0), 0.5 * (1.0 - smoothstep(0.08, 0.2, r.z)));

  float present = max(step(r.x, density), sparkle);
  // Fade the whole layer out where a cell is only a few pixels across (a small reflection target).
  float resolved = 1.0 - smoothstep(0.14, 0.28, px * n / 1.5708);
  return tint * (present * resolved * twinkle * (point + sparkle * arms));
}

// Height of ridge vertex k (an integer in 0..cols-1), periodic over cols: a slow swell per massif
// times a per-vertex factor, so peaks are jagged. The caller subtracts the range's sink, which puts
// the low stretches under the horizon. The +0.5 keeps the divisions away from whole numbers, so an inexact
// GPU divide cannot move a vertex to the wrong cell.
float sky_peak(float k, float cols, float massif, float seed) {
  float j = floor((k + 0.5) / massif);
  float g = (k - j * massif) / massif;
  float groups = cols / massif;
  float j1 = j + 1.0;
  j1 -= groups * floor((j1 + 0.5) / groups);
  float m = mix(sc_hash21(vec2(j, seed)), sc_hash21(vec2(j1, seed)), g * g * (3.0 - 2.0 * g));
  float r = sc_hash21(vec2(k, seed + 0.5));
  return (0.12 + 0.88 * m * m) * (0.5 + 0.5 * r);
}

vec4 sky_range0(float x, vec2 gx, float t, vec2 gt, float px, float amp, vec3 fill, vec3 line) {
  float i = floor(x);
  float fx = x - i;
  float ka = i - 150.0 * floor((i + 0.5) / 150.0);
  float kb = ka + 1.0;
  kb -= 150.0 * floor((kb + 0.5) / 150.0);
  float topA = SKY_BASE + amp * (sky_peak(ka, 150.0, 6.0, 11.0) - 0.12);
  float topB = SKY_BASE + amp * (sky_peak(kb, 150.0, 6.0, 11.0) - 0.12);
  float top = mix(topA, topB, fx);

  // Silhouette and ridge line, from the distance to the ridge in pixels.
  float ePx = (t - top) / max(length(gt - (topB - topA) * gx), 1e-7);
  float cover = 1.0 - smoothstep(-0.7, 0.7, ePx);
  float wRidge = 0.00085 / px;
  float ridge = sky_linePx(ePx + max(wRidge, 1.0), wRidge);

  float spanA = topA - SKY_FOOT;
  float spanB = topB - SKY_FOOT;
  float edge = min(fx, 1.0 - fx) / max(length(gx), 1e-7);   // pixels to the nearest mesh edge; columns first
  float facet = 0.0;
  float lowA = SKY_FOOT;
  float above = step(SKY_FOOT, t);
  float uPrev = 0.0;
  for (int k = 1; k <= 5; k++) {
    float s = float(k) / 5.0;
    float uk = s / (0.5 + s * (1.0 - 0.5));
    float wob = k < 5 ? 0.42 * (uk - uPrev) : 0.0;
    float hiA = SKY_FOOT + spanA * (uk + wob * (2.0 * sc_hash21(vec2(ka, 11.0 + float(k))) - 1.0));
    float hiB = SKY_FOOT + spanB * (uk + wob * (2.0 * sc_hash21(vec2(kb, 11.0 + float(k))) - 1.0));
    float er = t - mix(hiA, hiB, fx);
    float inRow = above * step(er, 0.0);
    edge = min(edge, abs(er) / max(length(gt - (hiB - hiA) * gx), 1e-7));
    float ed = t - mix(lowA, hiB, fx);
    edge = min(edge, mix(1e4, abs(ed) / max(length(gt - (hiB - lowA) * gx), 1e-7), inRow));
    facet = mix(facet, float(k) * 2.0 + step(0.0, ed), inRow);
    above = step(0.0, er);
    lowA = hiA;
    uPrev = uk;
  }

  // The mesh fades out where a cell is only a couple of pixels across.
  float cellPx = min(1.0 / max(length(gx), 1e-7), (top - SKY_FOOT) / (5.0 * max(length(gt), 1e-7)));
  float mesh = sky_linePx(edge, 0.00042 / px) * smoothstep(2.0, 5.0, cellPx);

  float u = clamp((t - SKY_FOOT) / max(top - SKY_FOOT, 1e-4), 0.0, 1.0);
  float shade = 0.6 + 0.8 * sc_hash21(vec2(ka * 32.0 + facet, 11.0 + 0.25));
  vec3 c = fill * shade * (0.7 + 0.6 * u);
  c += line * uGlow * (ridge * 1.5 + mesh * 0.7 * (0.4 + 0.6 * u));
  return vec4(c, cover);
}

vec4 sky_range1(float x, vec2 gx, float t, vec2 gt, float px, float amp, vec3 fill, vec3 line) {
  float i = floor(x);
  float fx = x - i;
  float ka = i - 180.0 * floor((i + 0.5) / 180.0);
  float kb = ka + 1.0;
  kb -= 180.0 * floor((kb + 0.5) / 180.0);
  float topA = SKY_BASE + amp * (sky_peak(ka, 180.0, 6.0, 23.0) - 0.2);
  float topB = SKY_BASE + amp * (sky_peak(kb, 180.0, 6.0, 23.0) - 0.2);
  float top = mix(topA, topB, fx);

  // Silhouette and ridge line, from the distance to the ridge in pixels.
  float ePx = (t - top) / max(length(gt - (topB - topA) * gx), 1e-7);
  float cover = 1.0 - smoothstep(-0.7, 0.7, ePx);
  float wRidge = 0.00085 / px;
  float ridge = sky_linePx(ePx + max(wRidge, 1.0), wRidge);

  float spanA = topA - SKY_FOOT;
  float spanB = topB - SKY_FOOT;
  float edge = min(fx, 1.0 - fx) / max(length(gx), 1e-7);   // pixels to the nearest mesh edge; columns first
  float facet = 0.0;
  float lowA = SKY_FOOT;
  float above = step(SKY_FOOT, t);
  float uPrev = 0.0;
  for (int k = 1; k <= 4; k++) {
    float s = float(k) / 4.0;
    float uk = s / (0.55 + s * (1.0 - 0.55));
    float wob = k < 4 ? 0.42 * (uk - uPrev) : 0.0;
    float hiA = SKY_FOOT + spanA * (uk + wob * (2.0 * sc_hash21(vec2(ka, 23.0 + float(k))) - 1.0));
    float hiB = SKY_FOOT + spanB * (uk + wob * (2.0 * sc_hash21(vec2(kb, 23.0 + float(k))) - 1.0));
    float er = t - mix(hiA, hiB, fx);
    float inRow = above * step(er, 0.0);
    edge = min(edge, abs(er) / max(length(gt - (hiB - hiA) * gx), 1e-7));
    float ed = t - mix(lowA, hiB, fx);
    edge = min(edge, mix(1e4, abs(ed) / max(length(gt - (hiB - lowA) * gx), 1e-7), inRow));
    facet = mix(facet, float(k) * 2.0 + step(0.0, ed), inRow);
    above = step(0.0, er);
    lowA = hiA;
    uPrev = uk;
  }

  // The mesh fades out where a cell is only a couple of pixels across.
  float cellPx = min(1.0 / max(length(gx), 1e-7), (top - SKY_FOOT) / (4.0 * max(length(gt), 1e-7)));
  float mesh = sky_linePx(edge, 0.00042 / px) * smoothstep(2.0, 5.0, cellPx);

  float u = clamp((t - SKY_FOOT) / max(top - SKY_FOOT, 1e-4), 0.0, 1.0);
  float shade = 0.6 + 0.8 * sc_hash21(vec2(ka * 32.0 + facet, 23.0 + 0.25));
  vec3 c = fill * shade * (0.7 + 0.6 * u);
  c += line * uGlow * (ridge * 1.9 + mesh * 0.8 * (0.4 + 0.6 * u));
  return vec4(c, cover);
}

vec4 sky_range2(float x, vec2 gx, float t, vec2 gt, float px, float amp, vec3 fill, vec3 line) {
  float i = floor(x);
  float fx = x - i;
  float ka = i - 210.0 * floor((i + 0.5) / 210.0);
  float kb = ka + 1.0;
  kb -= 210.0 * floor((kb + 0.5) / 210.0);
  float topA = SKY_BASE + amp * (sky_peak(ka, 210.0, 7.0, 37.0) - 0.3);
  float topB = SKY_BASE + amp * (sky_peak(kb, 210.0, 7.0, 37.0) - 0.3);
  float top = mix(topA, topB, fx);

  // Silhouette and ridge line, from the distance to the ridge in pixels.
  float ePx = (t - top) / max(length(gt - (topB - topA) * gx), 1e-7);
  float cover = 1.0 - smoothstep(-0.7, 0.7, ePx);
  float wRidge = 0.00085 / px;
  float ridge = sky_linePx(ePx + max(wRidge, 1.0), wRidge);

  float spanA = topA - SKY_FOOT;
  float spanB = topB - SKY_FOOT;
  float edge = min(fx, 1.0 - fx) / max(length(gx), 1e-7);   // pixels to the nearest mesh edge; columns first
  float facet = 0.0;
  float lowA = SKY_FOOT;
  float above = step(SKY_FOOT, t);
  float uPrev = 0.0;
  for (int k = 1; k <= 3; k++) {
    float s = float(k) / 3.0;
    float uk = s / (0.65 + s * (1.0 - 0.65));
    float wob = k < 3 ? 0.35 * (uk - uPrev) : 0.0;
    float hiA = SKY_FOOT + spanA * (uk + wob * (2.0 * sc_hash21(vec2(ka, 37.0 + float(k))) - 1.0));
    float hiB = SKY_FOOT + spanB * (uk + wob * (2.0 * sc_hash21(vec2(kb, 37.0 + float(k))) - 1.0));
    float er = t - mix(hiA, hiB, fx);
    float inRow = above * step(er, 0.0);
    edge = min(edge, abs(er) / max(length(gt - (hiB - hiA) * gx), 1e-7));
    float ed = t - mix(lowA, hiB, fx);
    edge = min(edge, mix(1e4, abs(ed) / max(length(gt - (hiB - lowA) * gx), 1e-7), inRow));
    facet = mix(facet, float(k) * 2.0 + step(0.0, ed), inRow);
    above = step(0.0, er);
    lowA = hiA;
    uPrev = uk;
  }

  // The mesh fades out where a cell is only a couple of pixels across.
  float cellPx = min(1.0 / max(length(gx), 1e-7), (top - SKY_FOOT) / (3.0 * max(length(gt), 1e-7)));
  float mesh = sky_linePx(edge, 0.00042 / px) * smoothstep(2.0, 5.0, cellPx);

  float u = clamp((t - SKY_FOOT) / max(top - SKY_FOOT, 1e-4), 0.0, 1.0);
  float shade = 0.6 + 0.8 * sc_hash21(vec2(ka * 32.0 + facet, 37.0 + 0.25));
  vec3 c = fill * shade * (0.7 + 0.6 * u);
  c += line * uGlow * (ridge * 1.7 + mesh * 0.8 * (0.4 + 0.6 * u));
  return vec4(c, cover);
}

void main() {
  vec3 d = normalize(vDir);
  // Derivatives first, before anything branches on the direction (Agents.md, "Custom GLSL").
  vec3 ddx = dFdx(d);
  vec3 ddy = dFdy(d);
  float px = max(max(length(ddx), length(ddy)), 1e-6);
  float lenXZ = max(length(d.xz), 1e-4);
  float tanEl = d.y / lenXZ;
  vec2 gt = vec2(dFdx(tanEl), dFdy(tanEl));

  float hp = max(d.y, 0.0);
  vec2 sunXZ = uSunDir.xz / max(length(uSunDir.xz), 1e-4);
  float cs = dot(d.xz, sunXZ) / lenXZ;
  float sunward = 0.5 + 0.5 * cs;
  float warm = sunward * sunward;
  vec3 fogCol = sc_fogColor(d);

  // ---- gradient: indigo overhead, through the mid colour, to a hot horizon that is hottest sunward.
  vec3 horizon = mix(uSkyHorizon * 0.42 + uSkyMid * 0.9, uSkyHorizon, warm);
  float kH = mix(16.0, 10.5, warm);
  vec3 col = uSkyTop + (uSkyMid - uSkyTop) * exp(-hp * 4.2) + (horizon - uSkyMid) * exp(-hp * kH);

  // ---- the sun's frame: right, up, and the offset of this direction from the sun.
  vec3 sunRight = normalize(vec3(-uSunDir.z, 0.0, uSunDir.x) + vec3(1e-6, 0.0, 0.0));
  vec3 sunUp = cross(sunRight, uSunDir);
  vec3 off = d - uSunDir;
  float rd = length(off);
  float outside = max(rd - SKY_SUN_R, 0.0) / SKY_SUN_R;

  // ---- stars: two layers on a cube grid, plus a faint dense band.
  float starMask = smoothstep(0.02, 0.3, d.y) * smoothstep(1.15, 3.2, rd / SKY_SUN_R);
  if (starMask > 0.0) {
    float amount = clamp(uStars, 0.0, 2.5);
    float bandD = dot(d, vec3(0.4211809623631152, 0.5214621438781426, -0.742080743211203)) / 0.17;
    float band = exp(-bandD * bandD);
    vec3 q;
    float face = sky_face(d, q);
    vec2 w = atan(q.xy / q.z) * (4.0 / SC_PI);
    // Patchy density, so the fine layer clusters and thins instead of showing its grid.
    float clump = sky_noise3(d * 5.5 + 9.0);
    vec3 starCol = sky_starLayer(q, w, face, 96.0, 1.0, px,
      min(0.3 * amount * (0.25 + 1.5 * clump * clump + 1.2 * band), 0.6), 0.0, vec3(0.00045, 0.0012, 1.5));
    starCol += sky_starLayer(q, w, face, 18.0, 3.0, px,
      min(0.5 * amount, 0.96), 0.035 * min(amount, 1.5), vec3(0.0009, 0.0021, 5.0));
#if SKY_DETAIL > 0
    float bandGlow = band * (0.3 + 0.7 * sky_noise3(d * 7.0 + 3.0)) * (0.5 + clump);
#else
    float bandGlow = band * 0.6;
#endif
    float brightness = 0.35 + 0.65 * min(amount, 1.4);
    col += starMask * (starCol * brightness + mix(uSkyMid, sc_neon(2.0), 0.5) * bandGlow * 0.07 * amount);
  }

  // ---- thin cloud streaks low in the sky, drifting round the vertical axis.
  float streakMask = smoothstep(0.012, 0.07, d.y) * (1.0 - smoothstep(0.12, 0.34, d.y));
  if (streakMask > 0.0) {
    float drift = uLoop * SC_TAU * 1.0;
    vec2 rot = vec2(cos(drift), sin(drift));
    vec3 pc = vec3(d.x * rot.x - d.z * rot.y, d.y * 11.0, d.x * rot.y + d.z * rot.x) * 4.2;
#if SKY_DETAIL > 0
    float cloud = 0.62 * sky_noise3(pc) + 0.38 * sky_noise3(pc * 2.07 + 11.3);
#else
    float cloud = sky_noise3(pc);
#endif
    float streak = smoothstep(0.52, 0.8, cloud) * streakMask;
    col += mix(uSkyHorizon, uSunBottom * 0.22, 0.45) * mix(0.3, 1.0, warm) * streak * 0.22;
  }

  // ---- the sun: halo, then the disc with its scrolling cut-outs, through which the sky shows.
  vec3 behind = col;
  col += mix(uSunBottom, uSunTop, 0.12) * (0.07 * exp(-outside * 2.8) + 0.028 * exp(-outside * 0.75));
  float disc = 1.0 - smoothstep(-0.75, 0.75, (rd - SKY_SUN_R) / px);
  float v = dot(off, sunUp) / SKY_SUN_R;                      // 1 at the top of the disc, -1 at the bottom
  float vHorizon = clamp(-uSunDir.y / SKY_SUN_R, -1.0, 0.1);  // where the horizon crosses the disc
  float depth = clamp((0.5 - v) / (0.5 - vHorizon), 0.0, 1.0);
  float stripeX = v / 0.17 + fract(uLoop * 384.0);
  float gap = sc_pulse(stripeX, 0.66 * sqrt(depth), px / (SKY_SUN_R * 0.17));
  float gv = clamp((v - vHorizon) / (1.0 - vHorizon), 0.0, 1.0);
  vec3 sunCol = mix(uSunBottom * 0.42, uSunTop * 0.85, pow(gv, 2.2));
  col = mix(col, mix(sunCol, behind * 0.75, gap), disc);

  // ---- wireframe mountains, far range first.
  float az = atan(d.x, -d.z + 1e-9) / SC_TAU;
  if (tanEl > SKY_FOOT - 0.01 && tanEl < SKY_MOUNTAIN_TOP) {
    vec2 gaz = vec2(d.x * ddx.z - d.z * ddx.x, d.x * ddy.z - d.z * ddy.x) / (lenXZ * lenXZ * SC_TAU);
    float flank = smoothstep(-0.4, 0.85, cs);
    float notch = exp(-(1.0 - cs) / 0.014);
    vec3 fillNear = uAmbient * 0.25 + uSkyTop * 0.35;
    vec3 fillFar = mix(fillNear, fogCol, 0.4);

    {
      float env = mix(0.5, 1.0, flank) * (1.0 - 0.76 * notch);
      vec4 m = sky_range0(az * 150.0, gaz * 150.0, tanEl, gt, px, 0.18 * env,
        mix(fillNear, fillFar, 1.0), sc_neon(2.0));
      col = mix(col, m.rgb, m.a);
    }

    {
      float env = mix(0.6, 1.0, flank) * (1.0 - 0.68 * notch);
      vec4 m = sky_range1(az * 180.0, gaz * 180.0, tanEl, gt, px, 0.11 * env,
        mix(fillNear, fillFar, 0.5), sc_neon(0.0));
      col = mix(col, m.rgb, m.a);
    }

    {
      float env = mix(0.75, 1.0, flank) * (1.0 - 0.45 * notch);
      vec4 m = sky_range2(az * 210.0, gaz * 210.0, tanEl, gt, px, 0.055 * env,
        mix(fillNear, fillFar, 0.0), sc_neon(1.0));
      col = mix(col, m.rgb, m.a);
    }
  }

  // ---- haze: the sky rises out of the fog colour in a thin band at the horizon, and is fog below it.
  col = mix(fogCol, col, exp(-0.009 / max(hp, 1e-5)));

  // ---- distant city lights: soft dots in a few rows along the horizon line.
  vec2 lg = vec2(az * SKY_LIGHT_COLS, (tanEl + 0.0015) / SKY_LIGHT_ROW_H);
  if (lg.y >= 0.0 && lg.y < SKY_LIGHT_ROWS) {
    vec2 lcell = floor(lg);
    vec2 lf = lg - lcell - 0.5;
    float lk = lcell.x - SKY_LIGHT_COLS * floor((lcell.x + 0.5) / SKY_LIGHT_COLS);
    vec3 l1 = sc_hash33(vec3(lk, lcell.y, 91.0));
    vec3 l2 = sc_hash33(vec3(lk, lcell.y, 92.0));
    float rowShare = 1.0 - 0.7 * lcell.y / SKY_LIGHT_ROWS;
    vec2 lo = (lf - (l2.xy - 0.5) * 0.5) * vec2(SC_TAU / SKY_LIGHT_COLS, SKY_LIGHT_ROW_H);
    float lrad = max(0.0006, 0.8 * px);
    vec2 lw = 1.0 - 4.0 * lf * lf;
    float spot = exp(-dot(lo, lo) / (lrad * lrad)) * lw.x * lw.x * lw.y * lw.y;
    float lb = l1.y * l1.y;
    lb = 0.3 + 2.4 * lb * lb;
    float lt = 0.72 + 0.28 * uMotion * sin(uTime * (1.5 + 4.0 * l1.z) + l1.z * 61.0);
    vec3 lightCol = mix(sc_neon(3.0), sc_neon(4.0), 0.5);
    lightCol = mix(lightCol, sc_neon(0.0), step(0.45, l2.z));
    lightCol = mix(lightCol, sc_neon(1.0), step(0.66, l2.z));
    lightCol = mix(lightCol, sc_neon(7.0), step(0.86, l2.z));
    float lres = smoothstep(1.2, 2.4, SKY_LIGHT_ROW_H / px);
    float lit = step(l1.x, 0.55 * rowShare) * spot * lb * lt;
    col += lightCol * (uGlow * 1.6 * mix(0.06 * rowShare, lit, lres));
  }

  gl_FragColor = vec4(max(col, 0.0), 1.0);
}
