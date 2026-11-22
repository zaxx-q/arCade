// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/ground/), adapted to GLSL 3.30 only by the prelude files.

varying vec3 vP;
void main() {
  vP = position;
  gl_Position = sc_project(position);
}
