// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 by the prelude files; the one other change is sl_pull() below.


attribute vec2 aRoad;   // axis, grid line
varying vec2 vRel;      // render-space x and z
varying float vAcross;  // metres from the road's centre line
flat varying vec2 vRoad;
// The web version draws the quads that lie on the road with a polygon offset towards the camera (factor -2,
// units -8) so they do not fight the ground's depth. rlgl has no polygon offset: the same pull in clip space.
vec4 sl_pull(vec4 clip) {
  clip.z -= clip.w * 2.5e-6;
  return clip;
}

void main() {
  float axis = aRoad.x, line = aRoad.y;
  float halfRoad = sc_roadHalf(sc_isAvenue(line));
  vec2 centre = sc_wrap(vec2(line * SC_CELL));
  float along = position.x * uFar.y;
  float across = position.y * halfRoad;
  vec3 p = axis < 0.5 ? vec3(along, 0.04, centre.y + across) : vec3(centre.x + across, 0.04, along);
  vRel = p.xz;
  vAcross = across;
  vRoad = aRoad;
  gl_Position = sl_pull(sc_project(p));
}
