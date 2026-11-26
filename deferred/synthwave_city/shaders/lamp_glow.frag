// Shader text taken from the web version (src/layers/ground/), adapted to GLSL 3.30 only by the prelude files.

varying vec2 vUv;
flat varying float vCol;
flat varying float vGain;
flat varying float vFar;
void main() {
  float r2 = dot(vUv, vUv);
  // A sprite held at its minimum size gets a broader profile, so a pixel grid can sample it without twinkling.
  float halo = exp(-r2 * mix(5.5, 2.3, vFar)) * (1.0 - smoothstep(0.55, 1.0, sqrt(r2)));
  float core = exp(-r2 * 110.0);
  vec3 lamp = sc_neon(vCol);
  // Up close: a soft halo round a small hot core. Far away the core would be under a pixel, so its
  // light is spread over the halo instead.
  vec3 col = lamp * (halo * 0.55)
           + mix(lamp, sc_neon(7.0), 0.6) * mix(core * 6.0, halo * 1.6, vFar);
  gl_FragColor = vec4(col * (vGain * uGlow), 1.0);
}
