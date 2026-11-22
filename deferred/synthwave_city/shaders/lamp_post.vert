// Shader text taken from the web version (src/layers/ground/), adapted to GLSL 3.30 only by the prelude files.
// Base of the pole in lamp-pattern coordinates (plus the lattice offset), and the unit direction to the road.
attribute vec4 aLamp;
// Head height, palette index.
attribute vec2 aInfo;
attribute float aPart;
// The point on the centre line of the pole that this vertex surrounds.
attribute vec3 aAxis;
uniform vec2 uPostFade;
varying vec3 vP;
varying float vPart;
varying float vUp;
flat varying float vCol;
void main() {
  // The layout repeats every G_PATTERN metres, so a small lattice of posts that follows the camera in
  // whole pattern steps is indistinguishable from a post at every lamp; the fade hides its edge.
  vec2 snap = floor(uCam.xz / G_PATTERN + 0.5) * G_PATTERN;
  vec3 anchor = vec3(snap.x + aLamp.x, 0.0, snap.y + aLamp.y);
  vec2 rel = sc_wrap(anchor.xz);
  float shown = 1.0 - smoothstep(uPostFade.x, uPostFade.y, length(rel));
  // The mesh is modelled at one height; everything above the split rides up to this lamp's height.
  vec3 lift = vec3(0.0, (aInfo.x - G_LAMP_H0) * step(G_LAMP_SPLIT, aAxis.y), 0.0);
  vec3 axis = aAxis + lift;
  vec3 radial = position - aAxis;
  // Keep the pole at least G_POST_MINPX pixels thick: a thinner one breaks up into crawling dots.
  float metresPerPixel = 2.0 * length(vec3(rel.x, cameraPosition.y - axis.y, rel.y)) / (uResolution.y * projectionMatrix[1][1]);
  float thicken = max(1.0, 0.5 * G_POST_MINPX * metresPerPixel / max(length(radial), 1e-4));
  vec3 l = axis + radial * thicken;
  vUp = l.y / aInfo.x;
  l = vec3(l.x * aLamp.z - l.z * aLamp.w, l.y, l.x * aLamp.w + l.z * aLamp.z) * shown;
  vec3 p = sc_place(anchor, l);
  vP = p;
  vPart = aPart;
  vCol = aInfo.y;
  gl_Position = sc_project(p);
}
