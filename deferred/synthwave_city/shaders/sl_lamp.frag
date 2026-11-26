// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 only by the prelude files.


varying vec2 vUv;
flat varying vec3 vColor;
flat varying vec2 vShape;
void main() {
  float r2 = dot(vUv, vUv);
  float core = exp(-r2 * vShape.x);
  float halo = exp(-r2 * 3.2) * 0.16;
  // A thin horizontal flare, as an anamorphic lens would draw.
  float streak = vShape.y * exp(-vUv.y * vUv.y * 110.0) * exp(-vUv.x * vUv.x * 2.4) * 0.3;
  float edge = 1.0 - smoothstep(0.7, 1.0, sqrt(r2));
  gl_FragColor = vec4(vColor * (core + halo + streak) * edge, 1.0);
}
