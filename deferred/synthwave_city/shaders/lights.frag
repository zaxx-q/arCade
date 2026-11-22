// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/buildings/), adapted to GLSL 3.30 only by the prelude files.
varying vec2 vCorner;
varying vec3 vColor;
void main() {
  float r2 = dot(vCorner, vCorner);
  float halo = max(1.0 - r2, 0.0);
  float core = exp(-r2 * 22.0);
  gl_FragColor = vec4(vColor * (core * 7.0 + halo * halo * 0.9), 1.0);
}
