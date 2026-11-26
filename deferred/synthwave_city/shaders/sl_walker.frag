// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 only by the prelude files.


varying vec3 vP;
varying vec2 vQ;
flat varying vec4 vBody;
flat varying vec4 vLook;
flat varying vec3 vAccent;
flat varying vec3 vRim;

// Distance to a limb: a segment from a to b whose radius runs from ra to rb.
float sl_limb(vec2 p, vec2 a, vec2 b, float ra, float rb) {
  vec2 pa = p - a, ba = b - a;
  float t = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
  return length(pa - ba * t) - mix(ra, rb, t);
}

void main() {
  vec2 p = vQ;
  float px = fwidth(vQ.x) + 1e-5;
  vec2 fw = vec2(px, fwidth(vQ.y) + 1e-5);
  float h = vBody.x, swing = vBody.y;
  float k = h / 1.75;
  p.y -= vBody.z;

  // Head, neck, shoulders, a torso that narrows to the hips.
  float d = length(p - vec2(0.0, h - 0.11 * k)) - 0.1 * k;
  d = min(d, sl_limb(p, vec2(0.0, 0.8 * h), vec2(0.0, 0.88 * h), 0.05 * k, 0.045 * k));
  d = min(d, sl_limb(p, vec2(-0.11 * k, 0.79 * h), vec2(0.11 * k, 0.79 * h), 0.065 * k, 0.065 * k));
  d = min(d, sl_limb(p, vec2(0.0, 0.75 * h), vec2(0.0, 0.54 * h), 0.15 * k, 0.115 * k));
  if (vLook.y > 0.5) {
    // A long coat.
    d = min(d, sl_limb(p, vec2(0.0, 0.56 * h), vec2(0.0, 0.31 * h), 0.13 * k, 0.185 * k));
  }
  // Legs swing from the hips and the arms swing against them.
  float reach = swing * 0.24 * k;
  d = min(d, sl_limb(p, vec2(-0.05 * k, 0.52 * h), vec2(reach - 0.03 * k, 0.05 + max(swing, 0.0) * 0.03), 0.095 * k, 0.05 * k));
  d = min(d, sl_limb(p, vec2(0.05 * k, 0.52 * h), vec2(0.03 * k - reach, 0.05 + max(-swing, 0.0) * 0.03), 0.095 * k, 0.05 * k));
  d = min(d, sl_limb(p, vec2(-0.17 * k, 0.78 * h), vec2((-0.2 - swing * 0.12) * k, 0.47 * h), 0.05 * k, 0.038 * k));
  d = min(d, sl_limb(p, vec2(0.17 * k, 0.78 * h), vec2((0.2 + swing * 0.12) * k, 0.47 * h), 0.05 * k, 0.038 * k));
  float canopy = 1.0;
  if (vLook.x > 0.5) {
    // An umbrella: a dome over the head on a thin pole.
    canopy = max(length(p - vec2(0.0, h + 0.02)) - 0.52, (h + 0.2) - p.y);
    d = min(d, sl_limb(p, vec2(0.2 * k, 0.5 * h), vec2(0.0, h + 0.4), 0.012, 0.012));
    d = min(d, canopy);
  }
  // Keep a far figure at least a pixel wide.
  d -= 0.35 * px;
  if (d > 0.0) discard;

  vec3 col = mix(vec3(0.010, 0.008, 0.024), vec3(0.045, 0.04, 0.065), vLook.z);
  // The block's neon catches the outline: a hairline close by, the whole figure once it is a few pixels tall.
  float rim = smoothstep(-max(0.02, 1.2 * px), 0.0, d);
  col += vRim * rim * 0.32;
  vec3 glow = vec3(0.0);
  float v = vBody.w;
  float chest = step(0.56 * h, p.y) * (1.0 - step(0.77 * h, p.y));
  if (v < 0.28) {
    // An open jacket with a lit seam.
    glow += vAccent * sc_line(p.x, 0.012, px) * chest * 2.6;
  } else if (v < 0.5) {
    // A visor.
    glow += vAccent * sc_line(p.x, 0.08 * k, fw.x) * sc_line(p.y - (h - 0.1 * k), 0.016, fw.y) * 3.2;
  } else if (v < 0.7) {
    // Lit soles.
    glow += vAccent * sc_line(p.y - 0.045, 0.03, fw.y) * 2.6;
  } else if (v < 0.84) {
    // A screen held in the hand.
    glow += sc_neon(7.0) * sc_line(p.x - 0.14 * k, 0.03, fw.x) * sc_line(p.y - 0.6 * h, 0.05, fw.y) * 3.0;
  } else {
    // Piping down both sides of a jacket.
    glow += vAccent * sc_line(abs(p.x) - 0.12 * k, 0.011, fw.x) * chest * 2.2;
  }
  if (vLook.x > 0.5) {
    float dome = step(h + 0.2, p.y);
    glow += vAccent * dome * (sc_line(canopy + 0.016, 0.016, px) * 2.4 + 0.1 * (1.0 - step(0.0, canopy)));
  }
  col += glow * uGlow;
  gl_FragColor = vec4(sc_fog(col, vP), 1.0);
}
