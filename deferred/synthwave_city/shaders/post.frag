#version 330 core
// Written after pmndrs/postprocessing (zlib) and three.js (MIT) algorithms; notices in THIRD_PARTY_NOTICES.md.
// The last pass, in the web version's order: chromatic aberration of the HDR frame, bloom added with the
// SCREEN blend, the grade (exposure, vignette, neon curve, grain), the sRGB encoding and three.js's dither.
uniform sampler2D inputBuffer;
uniform sampler2D bloomBuffer;
uniform vec2 resolution;
uniform vec2 caOffset;        // chromatic aberration, 0 = off
uniform float caModulation;   // radial modulation offset
uniform float bloomIntensity;
uniform float uExposure;
uniform float uHold;
uniform float uVignette;
uniform float uGrain;
uniform float uSeed;
uniform float uCrt;
uniform float uRaw;           // 1: the web version's no-post fallback (ACES, gamma 2.2), for A/B tests
in vec2 vUv;
out vec4 fragColor;

vec3 gradeNeon(vec3 c) {
  vec3 perChannel = 1.0 - exp(-c);
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  vec3 held = c * ((1.0 - exp(-l)) / max(l, 1e-4));
  float peak = max(held.r, max(held.g, held.b));
  if (peak > 1.0) held = mix(held / peak, vec3(1.0), clamp((peak - 1.0) * 0.25, 0.0, 0.6));
  return clamp(mix(perChannel, held, uHold), 0.0, 1.0);
}

float gradeHash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float ditherRand(const in vec2 uv) {
  const float a = 12.9898, b = 78.233, c = 43758.5453;
  float dt = dot(uv.xy, vec2(a, b)), sn = mod(dt, 3.141592653589793);
  return fract(sin(sn) * c);
}

vec3 acesFit(vec3 x) {
  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
  vec2 uv = vUv;
  vec4 inputColor = texture(inputBuffer, uv);
  if (uRaw > 0.5) {
    fragColor = vec4(pow(acesFit(max(inputColor.rgb, 0.0) * uExposure), vec3(1.0 / 2.2)), 1.0);
    return;
  }

  // chromatic aberration (pmndrs ChromaticAberrationEffect, radial modulation)
  vec2 shift = caOffset * vec2(1.0, resolution.x / resolution.y);
  vec2 ra = inputColor.ra;
  vec2 ba = inputColor.ba;
  float d = max(distance(uv, vec2(0.5)) * 2.0 - caModulation, 0.0);
  if ((shift.x != 0.0 || shift.y != 0.0) && d > 0.0) {
    ra = texture(inputBuffer, mix(uv, uv + shift, d)).ra;
    ba = texture(inputBuffer, mix(uv, uv - shift, d)).ba;
  }
  vec3 dst = vec3(ra.x, inputColor.g, ba.x);

  // bloom, SCREEN blend
  vec3 src = texture(bloomBuffer, uv).rgb * bloomIntensity;
  vec3 c = dst + src - min(dst * src, 1.0);

  // grade
  c = max(c, 0.0) * uExposure;
  vec2 q = uv - 0.5;
  float v = dot(q, q);
  c *= 1.0 - uVignette * smoothstep(0.08, 0.62, v);
  c = gradeNeon(c);
  c = mix(c, c * c * (3.0 - 2.0 * c), 0.18);
  vec2 px = uv * resolution;
  if (uCrt > 0.0) {
    float line = 0.5 + 0.5 * cos(px.y * 3.14159265);
    c *= 1.0 - uCrt * 0.28 * line;
    c *= 1.0 + uCrt * 0.04 * cos(px.x * 2.0943951);
  }
  float g = gradeHash(px + uSeed * 61.0) - 0.5;
  c += g * uGrain * (0.02 + dot(c, vec3(0.333)));
  c = max(c, 0.0);

  // sRGB encoding (three.js sRGBTransferOETF) and its dither
  c = mix(pow(c, vec3(0.41666)) * 1.055 - vec3(0.055), c * 12.92, vec3(lessThanEqual(c, vec3(0.0031308))));
  float grid = ditherRand(gl_FragCoord.xy);
  vec3 shiftRGB = vec3(0.25 / 255.0, -0.25 / 255.0, 0.25 / 255.0);
  shiftRGB = mix(2.0 * shiftRGB, -2.0 * shiftRGB, grid);
  fragColor = vec4(c + shiftRGB, 1.0);
}
