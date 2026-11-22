// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 only by the prelude files.


varying vec2 vQ;
varying float vClear;
flat varying vec4 vKind;
flat varying vec3 vColor;
void main() {
  float id = vKind.x, halfL = vKind.y, halfW = vKind.z;
  float a;
  if (id < 0.5) {
    // Two beams merging into one widening pool.
    float t = clamp((vQ.x - halfL) / 20.0, 0.0, 1.0);
    float w = mix(0.7, 1.74, t);
    float y = abs(vQ.y) - 0.55 * (1.0 - t);
    a = smoothstep(0.0, 0.06, t) * pow(1.0 - t, 2.4) * exp(-y * y / (w * w));
  } else if (id < 1.5) {
    float t = clamp((-halfL - vQ.x) / 7.0, 0.0, 1.0);
    a = (1.0 - t) * (1.0 - t) * exp(-vQ.y * vQ.y * 1.1) * (1.0 - smoothstep(0.0, 0.5, vQ.x + halfL));
  } else {
    vec2 d = max(abs(vQ) - vec2(halfL - 0.2, halfW - 0.2), 0.0);
    a = 1.0 - smoothstep(0.0, 1.0, length(d) / vKind.w);
    a *= a;
  }
  gl_FragColor = vec4(vColor * a * vClear, 1.0);
}
