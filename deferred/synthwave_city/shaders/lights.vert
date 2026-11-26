// Shader text taken from the web version (src/layers/buildings/), adapted to GLSL 3.30 only by the prelude files.
// Instance: x, y, z, size.
attribute vec4 aSite;
// Instance: palette index, seed.
attribute vec2 aKind;
varying vec2 vCorner;
varying vec3 vColor;
void main() {
  vec3 centre = sc_place(aSite.xyz, vec3(0.0));
  float alive = step(0.0005, sc_farFade(sc_wrap(aSite.xz)));
  float seed = floor(aKind.y * 4096.0);
  vec2 h = sc_hash22(vec2(seed, 5.0));
  // Three habits: a slow beacon, a double flash, a steady lamp that breathes.
  float ph = fract(uTime * (0.55 + 0.35 * h.x) + h.y);
  float flash;
  if (h.x < 0.45) flash = smoothstep(0.0, 0.08, ph) * (1.0 - smoothstep(0.3, 0.5, ph));
  else if (h.x < 0.8) flash = step(ph, 0.06) + step(0.16, ph) * step(ph, 0.22);
  else flash = 0.75 + 0.25 * sin(ph * SC_TAU);
  float level = mix(0.7, 0.12 + flash, uMotion);
  // Faded by fog like any other light, and never smaller than a couple of pixels.
  float fog = sc_fogAmount(centre);
  vec3 c = sc_neon(aKind.x);
  vColor = c / pow(max(sc_luma(c), 0.05), 0.75) * (level * (1.0 - fog) * uGlow);
  vCorner = position.xy;
  float size = sc_minSize(centre, aSite.w * 3.2, 2.2) * alive;
  gl_Position = sc_project(sc_sprite(centre, position.xy, size));
}
