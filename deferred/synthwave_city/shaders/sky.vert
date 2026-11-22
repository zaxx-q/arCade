// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version, adapted to GLSL 3.30 only by the prelude files.

varying vec3 vDir;
void main() {
  vDir = position;
  // Rotation only: the dome is centred on whichever camera is drawing. z = w puts it on the far plane.
  vec4 p = projectionMatrix * vec4(mat3(viewMatrix) * position, 1.0);
  gl_Position = p.xyww;
}
