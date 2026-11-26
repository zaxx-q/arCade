// Shader text taken from the web version (src/layers/ground/), adapted to GLSL 3.30 only by the prelude files.
// Lamp head: tile x, height, tile z.
attribute vec3 aHead;
attribute float aCol;
varying vec2 vUv;
flat varying float vCol;
flat varying float vGain;
flat varying float vFar;
void main() {
  vec3 centre = sc_place(aHead, vec3(0.0));
  float k = sc_farFade(sc_wrap(aHead.xz));
  float size = sc_minSize(centre, G_SPRITE, G_SPRITE_MINPX);
  float ratio = G_SPRITE / size;
  // Held at a few pixels, a distant glow dims, but more slowly than its true size would have it.
  vGain = k * pow(ratio, 1.35) * (1.0 - sc_fogAmount(centre));
  vFar = 1.0 - ratio;
  vCol = aCol;
  vUv = position.xy;
  gl_Position = sc_project(sc_sprite(centre, position.xy, size * step(0.001, k)));
}
