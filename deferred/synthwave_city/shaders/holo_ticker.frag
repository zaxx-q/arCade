// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/holograms/), adapted to GLSL 3.30 only by the prelude files.


vec3 holo_ink(vec3 t, vec3 inkA, vec3 inkB) {
  return inkA * t.r + inkB * t.g + vec3(0.86, 0.95, 1.0) * (t.b * 1.3);
}

uniform sampler2D uAtlas;
varying vec3 vP;
varying vec3 vBand;
flat varying vec4 vRect;
flat varying vec4 vLook;
flat varying vec3 vNormal;

void main() {
  float h = vLook.w;
  float seed = vLook.z;
  vec3 inkA = sc_neon(vLook.x), inkB = sc_neon(vLook.y);
  vec2 metres = vec2(vBand.y, vBand.z * h);
  vec2 fwm = max(fwidth(metres), vec2(1e-5));
  float fe = max(fwm.x, fwm.y);
  // The text fills the middle of the band; rules run along its top and bottom.
  float tv = (vBand.z - 0.14) / 0.72;
  vec2 tuv = vec2(vBand.x, tv * vRect.w);
  vec2 gx = dFdx(tuv), gy = dFdy(tuv);
  float inText = step(0.0, tv) * step(tv, 1.0);
  vec3 t = textureGrad(uAtlas, vec2(vBand.x, vRect.y + clamp(tv, 0.0, 1.0) * vRect.w), gx, gy).rgb * inText;
  // LED dots, fading to their average once they are too small to resolve.
  float pitch = h / 26.0;
  vec2 cellUv = fract(metres / pitch) - 0.5;
  float dotMask = 1.0 - smoothstep(0.2, 0.47, length(cellUv));
  float led = mix(1.0, dotMask * 2.3, sc_detail(pitch, fe));
  float ey = min(vBand.z, 1.0 - vBand.z) * h;   // metres inside the band; negative in the halo
  float rule = sc_line(ey - 0.07 * h, 0.012 * h, fwm.y);
  float edgeFade = smoothstep(0.0, 0.03 * h, ey);
  float halo = exp(min(ey, 0.0) / (0.2 * h)) * smoothstep(-0.5 * h, -0.25 * h, ey) * (1.0 - edgeFade);
  float flick = 1.0 - uMotion * 0.08 * sc_noise(vec2(uTime * 8.0, seed));
  vec3 view = vec3(0.0, cameraPosition.y, 0.0) - vP;
  float graze = smoothstep(0.03, 0.25, abs(dot(normalize(view), vNormal)));
  // The band sits in front of lit megatower facades, so its text is pushed towards white-hot.
  vec3 lit = holo_ink(t, inkA, inkB);
  lit = lit * 3.4 + sc_luma(lit) * vec3(1.6);
  vec3 col = (lit * led + inkA * (0.1 + rule * 2.6)) * edgeFade + inkA * halo * 0.45;
  col *= (0.85 + 0.35 * uBeat.x) * flick * graze * uGlow * (1.0 - sc_fogAmount(vP));
  gl_FragColor = vec4(col, 1.0);
}
