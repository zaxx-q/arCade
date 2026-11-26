// Shader text taken from the web version (src/layers/holograms/), adapted to GLSL 3.30 only by the prelude files.


varying vec3 vBary;
varying vec3 vP;
varying vec3 vLocal;
varying float vMpp;
flat varying vec4 vLook;
flat varying vec4 vStyle;

void main() {
  float kind = vLook.z;
  float seed = vLook.w;
  float radius = vStyle.z;
  vec3 ink = sc_neon(vLook.x);
  vec3 fwb = fwidth(vBary);
  float fwy = max(fwidth(vP.y), 1e-5);
  float flick = 1.0 - uMotion * 0.1 * sc_noise(vec2(uTime * 7.0, seed));
  vec3 col;
  if (kind < 0.5) {
    // Distance to the nearest drawn edge, in pixels.
    vec3 px = vBary / max(fwb, vec3(1e-6));
    float dpx = min(px.x, min(px.y, px.z));
    float wpx = vStyle.x / max(vMpp, 1e-4);
    float edge = sc_line(dpx, wpx, 1.0);
    float halo = exp(-dpx / (wpx * 2.5 + 2.0)) * 0.2;
    // A plane of light sweeping up through the solid.
    float sweepY = mix(-1.25, 1.25, fract(uTime * 0.11 + seed * 0.173)) * radius;
    float sweep = exp(-abs(vLocal.y - sweepY) / (0.06 * radius));
    // Faces carry a little light, cut by raster lines.
    float period = max(0.9, radius * 0.045);
    float raster = sc_pulse(vP.y / period, 0.5, fwy / period);
    float face = vStyle.y * (0.25 + 0.75 * raster) * 0.42;
    col = ink * (edge * 2.8 + halo + face) * (1.0 + 2.2 * sweep) + vec3(0.8, 0.93, 1.0) * edge * (0.6 + 2.0 * sweep);
    col *= vLook.y * (1.0 + 0.25 * uBeat.x);
  } else if (kind < 1.5) {
    float r = length(vLocal.xy);
    float g = max(1.0 - r, 0.0);
    col = ink * (g * g * 0.11 + pow(g, 6.0) * 0.25) * (0.8 + 0.4 * uBeat.x);
  } else {
    // The projector cone: x is 0 at the plaza floor and 1 at the solid, y runs round it.
    float t = clamp(vBary.x, 0.0, 1.0);
    float streak = 0.5 + 0.5 * sc_noise(vec2(vBary.y * 60.0 + seed, uTime * 0.6 - t * 3.0));
    col = ink * streak * (0.07 + 0.5 * pow(1.0 - t, 3.0)) * (1.0 - smoothstep(0.8, 1.0, t)) * 0.6;
  }
  col *= flick * uGlow * (1.0 - sc_fogAmount(vP));
  gl_FragColor = vec4(col, 1.0);
}
