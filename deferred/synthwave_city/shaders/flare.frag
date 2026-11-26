// Shader text taken from the web version (src/layers/beams/), adapted to GLSL 3.30 only by the prelude files.

#define BM_SEARCHLIGHT 0.0
#define BM_PILLAR 1.0
#define BM_LASER 2.0
#define BM_PILLAR_HALO 8.0
#define BM_LASER_GLOW 4.0

varying vec2 vCorner;
varying vec3 vP;
flat varying vec4 vLook;
flat varying vec2 vGain;

void main() {
  float kind = vLook.w;
  float seed = floor(vLook.y * 4096.0);
  vec3 tint = sc_neon(vLook.x);
  float d = length(vCorner);
  float rim = max(1.0 - d, 0.0);
  float glow = rim * rim * rim;
  // A thin horizontal streak, the anamorphic flare of the era. Never thinner than a pixel.
  float sw = max(0.033, fwidth(vCorner.y));
  float sy = vCorner.y / sw;
  float streak = exp(-sy * sy) * (0.033 / sw) * rim * rim;
  float flick = 1.0 - 0.2 * uMotion * sc_noise(vec2(uTime * 5.0, seed));
  vec3 col;
  if (kind == BM_PILLAR) {
    float core = exp(-d * d * 150.0);
    float beat = 1.0 + 0.35 * uBeat.x * uMotion;
    col = mix(tint, vec3(1.0), 0.5) * core * 7.0 + tint * (glow * 0.34 + streak * 0.5) * beat;
  } else if (kind == BM_LASER) {
    float core = exp(-d * d * 30.0);
    col = mix(tint, vec3(1.0), 0.4) * core * (4.0 + 20.0 * vGain.y) + tint * glow * 0.5;
  } else {
    float core = exp(-d * d * 45.0);
    col = mix(tint, vec3(1.0), 0.5) * core * (4.5 + 5.0 * vGain.y) + tint * (glow * 0.35 + streak * (0.3 + 1.5 * vGain.y));
  }
  col *= vLook.z * vGain.x * flick * uGlow;
  col *= 1.0 - sc_fogAmount(vP);
  gl_FragColor = vec4(col, 1.0);
}
