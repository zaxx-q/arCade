// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version, adapted to GLSL 3.30 only by the prelude files.
// Vertex helpers (camera-facing sprites, projection).

vec4 sc_project(vec3 renderPos) { return projectionMatrix * viewMatrix * vec4(renderPos, 1.0); }

// Camera-facing sprite. centre: render space. corner: -1..1 on each axis. size: half-size in metres.
vec3 sc_sprite(vec3 centre, vec2 corner, float size) {
  vec3 right = vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
  vec3 up = vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
  return centre + (right * corner.x + up * corner.y) * size;
}
// Half-size in metres that covers at least minPixels pixels at this render-space position.
float sc_minSize(vec3 centre, float size, float minPixels) {
  float dist = length(centre - vec3(0.0, cameraPosition.y, 0.0));
  float metresPerPixel = 2.0 * dist / (uResolution.y * projectionMatrix[1][1]);
  return max(size, minPixels * metresPerPixel);
}
// A ribbon that turns about its own axis to face the camera (beams, trails).
// point: render-space position on the axis. axis: unit direction. side: -1..1 across. halfWidth: metres.
vec3 sc_axisBillboard(vec3 point, vec3 axis, float side, float halfWidth) {
  vec3 toCam = vec3(0.0, cameraPosition.y, 0.0) - point;
  vec3 right = cross(axis, toCam);
  float len = length(right);
  right = len > 1e-4 ? right / len : vec3(1.0, 0.0, 0.0);
  return point + right * side * halfWidth;
}
// 0 when the camera looks nearly along the axis (the ribbon is degenerate there), 1 side-on.
float sc_axisFacing(vec3 point, vec3 axis) {
  vec3 toCam = normalize(vec3(0.0, cameraPosition.y, 0.0) - point);
  return smoothstep(0.02, 0.25, length(cross(axis, toCam)));
}
