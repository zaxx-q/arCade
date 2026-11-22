// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/airtraffic/), adapted to GLSL 3.30 only by the prelude files.

#define AT_SPRITE 0.0
#define AT_RIBBON 1.0
#define AT_CONE 2.0
#define AT_POOL 3.0

// centroid: on a multisampled target a plain varying is evaluated at the pixel centre even when
// that lies outside the triangle. A trail seen end-on is a sliver, the extrapolated 'along' ran
// to minus thousands, exp() of it overflowed, and infinity times a zero fade is NaN, which the
// bloom then spreads into a black block. Centroid sampling keeps every value inside the triangle;
// the clamps below are the second lock on the same door.
centroid varying vec2 vUv;
centroid varying vec3 vColor;    // colour times intensity, fog and far fade already applied
centroid varying vec4 vShape;
flat varying float vKind;
void main() {
  vec3 col;
  vec2 uv = clamp(vUv, vec2(-1.0), vec2(1.0));
  if (vKind < 0.5) {
    // Sprite. vShape: core radius, streak strength, quad aspect, core gain.
    vec2 q = uv * vec2(vShape.z, 1.0);
    float r = length(q);
    float rc = max(vShape.x, 0.05);
    float core = exp(-(r * r) / (rc * rc)) * vShape.w;
    float halo = pow(max(1.0 - r, 0.0), 3.0);
    // The anamorphic streak of an 80s lens, across the whole quad.
    float streak = vShape.y * exp(-abs(uv.y) / (rc * 0.3)) * pow(max(1.0 - abs(uv.x), 0.0), 2.0) * min(vShape.w * 2.0, 1.0);
    col = vColor * (core * 6.0 + halo * 0.45 + streak * 1.2) + vec3(1.5) * core * core * sc_luma(vColor);
  } else if (vKind < 1.5) {
    // Trail or plume. vUv: across -1..1, along 0..1. vShape: line offset, line width (both in
    // units of the half-width), seed, length in metres.
    float a = clamp(uv.y, 0.0, 1.0);
    float sigma = max(vShape.y, 0.02);
    float d1 = (uv.x - vShape.x) / sigma;
    float d2 = (uv.x + vShape.x) / sigma;
    // Two hot exhaust lines inside a soft glow as wide as the ribbon.
    float glow = 1.0 - uv.x * uv.x;
    float prof = exp(-d1 * d1) + exp(-d2 * d2) + 0.16 * glow * glow;
    float fade = pow(max(1.0 - a, 0.0), 1.6) * smoothstep(0.0, 0.03, a);
    float flick = 0.8 + 0.2 * sc_noise(vec2(a * vShape.w * 0.3 - uTime * 7.0, floor(vShape.z * 4096.0 + 0.5)));
    col = vColor * prof * fade * flick * (1.0 + 0.4 * exp(-a * 30.0));
  } else if (vKind < 2.5) {
    // Searchlight cone. vShape.z: seed, vShape.w: length in metres.
    float a = clamp(uv.y, 0.0, 1.0);
    float across = 1.0 - uv.x * uv.x;
    float shimmer = 0.75 + 0.25 * sc_noise(vec2(a * vShape.w * 0.08 - uTime * 0.7, uv.x * 1.5 + vShape.z * 40.0));
    float along = mix(1.0, 0.16, smoothstep(0.0, 1.0, a)) * smoothstep(0.0, 0.01, a) * (1.0 - smoothstep(0.985, 1.0, a));
    col = vColor * across * across * along * shimmer;
  } else {
    // Pool of light on the road.
    float r = length(uv);
    float disc = 1.0 - smoothstep(0.5, 1.0, r);
    col = vColor * (disc * 0.8 + 0.6 * exp(-r * r * 9.0));
  }
  gl_FragColor = vec4(max(col, vec3(0.0)), 1.0);
}
