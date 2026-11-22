// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/holograms/), adapted to GLSL 3.30 only by the prelude files.


vec3 holo_ink(vec3 t, vec3 inkA, vec3 inkB) {
  return inkA * t.r + inkB * t.g + vec3(0.86, 0.95, 1.0) * (t.b * 1.3);
}

uniform sampler2D uAtlas;
varying vec2 vUv;
varying vec3 vP;
flat varying vec4 vRectA;
flat varying vec4 vRectB;
flat varying vec4 vLook;
flat varying vec4 vDim;
flat varying vec4 vFit;
flat varying vec3 vNormal;

void main() {
  float part = vDim.w;
  float seed = vDim.z;
  vec3 view = vec3(0.0, cameraPosition.y, 0.0) - vP;
  float dist = length(view);
  vec3 inkA = sc_neon(vLook.x), inkB = sc_neon(vLook.y);

  // Every derivative is taken here, before anything branches.
  vec2 uv = vUv;
  vec2 m = uv * vDim.xy;
  vec2 fwm = max(fwidth(m), vec2(1e-5));
  vec2 cuv = (uv - 0.5) * vFit.xy + 0.5;
  vec2 gx = dFdx(cuv) * vRectA.zw, gy = dFdy(cuv) * vRectA.zw;

  float flick = 1.0 - uMotion * (0.07 * sc_noise(vec2(uTime * 9.0, seed))
    + 0.3 * step(0.972, sc_hash21(vec2(seed + 5.0, floor(uTime * 12.0)))));
  float graze = smoothstep(0.04, 0.32, abs(dot(view / max(dist, 1e-3), vNormal)));
  // Flying through a panel should be a wash of light, not a white-out.
  float near = mix(0.14, 1.0, smoothstep(3.0, 44.0, dist));

  vec3 col;
  if (part < 0.5) {
    // ---- glitch: now and then a band of the image slips sideways
    float tq = floor(uTime * 6.0);
    float glitch = step(0.94, sc_hash21(vec2(seed, tq))) * uMotion;
    float bandY = sc_hash21(vec2(seed + 11.0, tq));
    float bandH = 0.03 + 0.1 * sc_hash21(vec2(seed + 23.0, tq));
    float inBand = glitch * (1.0 - smoothstep(bandH * 0.8, bandH, abs(uv.y - bandY)));
    cuv.x += (sc_hash21(vec2(seed + 37.0, tq)) - 0.5) * 0.3 * inBand;

    // ---- wipe between the two ads
    float style = vLook.w;
    float wc = style < 0.5 ? uv.x : style < 1.5 ? 1.0 - uv.y : style < 2.5 ? 0.5 * (uv.x + uv.y) : abs(uv.x - 0.5) * 2.0;
    float kk = vLook.z * 1.3 - 0.15;
    float showB = step(wc, kk);
    float wipeEdge = exp(-abs(wc - kk) * 38.0);
    vec4 rect = mix(vRectA, vRectB, showB);
    cuv.x += wipeEdge * 0.06 * (sc_hash21(vec2(floor(uv.y * 48.0), tq)) - 0.5);

    // ---- the ad, sampled twice for a red/cyan fringe that grows towards the edges
    vec2 off = abs(uv - 0.5) * 2.0;
    float fr = 0.0012 + 0.003 * pow(max(off.x, off.y), 3.0) + 0.03 * inBand + 0.02 * wipeEdge;
    vec2 ua = cuv + vec2(fr, 0.0), ub = cuv - vec2(fr, 0.0);
    float ma = step(0.0, ua.x) * step(ua.x, 1.0) * step(0.0, ua.y) * step(ua.y, 1.0);
    float mb = step(0.0, ub.x) * step(ub.x, 1.0) * step(0.0, ub.y) * step(ub.y, 1.0);
    vec3 ta = textureGrad(uAtlas, rect.xy + clamp(ua, 0.0, 1.0) * rect.zw, gx, gy).rgb * ma;
    vec3 tb = textureGrad(uAtlas, rect.xy + clamp(ub, 0.0, 1.0) * rect.zw, gx, gy).rgb * mb;
    vec3 ca = holo_ink(ta, inkA, inkB), cb = holo_ink(tb, inkA, inkB);
    vec3 img = vec3(ca.r, 0.5 * (ca.g + cb.g), cb.b);

    // ---- raster lines in panel space, box-filtered so they melt into their average far away
    float period = vDim.y / clamp(floor(vDim.y / 0.42), 24.0, 110.0);
    float scan = sc_pulse((m.y + uTime * 0.35) / period, 0.62, fwm.y / period);
    // Gone by the time a line is two pixels: the box filter alone still beats against the pixel grid there.
    scan = mix(0.62, scan, 1.0 - smoothstep(0.22, 0.5, fwm.y / period));
    float scanK = mix(0.42, 1.2, scan);
    // ---- a soft bar of brightness rolling up the image
    float roll = fract(uv.y * 0.7 - uTime * 0.09 + seed * 0.37);
    float bar = smoothstep(0.0, 0.05, roll) * (1.0 - smoothstep(0.05, 0.3, roll));

    // ---- frame, corner brackets, and the fade to nothing at the quad's edge
    float ex = min(m.x, vDim.x - m.x), ey = min(m.y, vDim.y - m.y);
    float e = min(ex, ey);
    float fe = max(fwm.x, fwm.y);
    float inset = clamp(0.022 * min(vDim.x, vDim.y), 0.4, 1.2);
    float lw = inset * 0.15;
    float frame = sc_line(e - inset, lw, fe);
    float reach = 0.17 * min(vDim.x, vDim.y);
    float corner = 1.0 - smoothstep(reach, reach + fe, max(ex, ey));
    float bracket = sc_line(e - inset, lw * 3.2, fe) * corner;
    float inside = smoothstep(inset - fe, inset + fe, e);
    float halo = exp(-abs(e - inset) / (inset * 0.8)) * 0.1;
    float edgeFade = smoothstep(0.0, inset * 0.6, e);

    // Far away a panel is only a few pixels: lift it so it still reads as a card of coloured light.
    float far = smoothstep(150.0, 1500.0, dist);
    vec3 veil = inkA * (0.03 + 0.035 * uv.y) * (1.0 + 4.0 * far);
    vec3 body = (img * 3.1 * (1.0 + 0.45 * bar + 0.7 * far) + veil * (1.0 + 2.5 * bar)) * scanK;
    body += (inkA + inkB + 0.6) * wipeEdge * 0.9;
    vec3 trim = (inkA * (frame * 1.7 + bracket * 2.4 + halo) + bracket * 0.7) * (1.0 + 0.4 * uBeat.x);
    col = (body * inside + trim) * edgeFade * vFit.z;
  } else if (part < 1.5) {
    // ---- the fan of light from the projector: brightest at the lamp, gone at the image
    // Clamped: under MSAA a varying can be extrapolated past the quad, and pow of a negative is NaN.
    float t = clamp(uv.y, 0.0, 1.0);
    float halfW = mix(vFit.w, 0.5 * vDim.x, t);
    float ax = abs(uv.x) / halfW;
    float across = 1.0 - smoothstep(0.35, 1.0, ax);
    float streak = 0.55 + 0.45 * sc_noise(vec2(uv.x / halfW * 9.0 + seed, uTime * 0.7));
    float fall = (0.06 + 0.6 * pow(1.0 - t, 2.2)) * smoothstep(0.0, 0.04, t) * (1.0 - smoothstep(0.85, 1.0, t));
    col = inkA * (across * streak * fall * 0.55);
  } else {
    // ---- the lamp: a slim bar with a white-hot middle
    float bar = exp(-abs(uv.y) * 7.0) * (1.0 - smoothstep(0.5, 0.62, abs(uv.x)));
    float hot = exp(-length(uv * vec2(2.4, 5.0)) * 3.2);
    float glow = exp(-length(uv * vec2(1.0, 2.0)) * 3.0) * 0.35 * (1.0 - smoothstep(0.6, 1.0, length(uv)));
    col = inkA * (bar * 2.2 + glow) + vec3(0.9, 0.97, 1.0) * hot * 5.0 * (1.0 + 0.3 * uBeat.x);
    near = 1.0;
  }
  col *= flick * graze * near * uGlow * (1.0 - sc_fogAmount(vP));
  gl_FragColor = vec4(col, 1.0);
}
