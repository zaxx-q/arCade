// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 only by the prelude files.


// The blue half of an emergency strobe; the palette has no pure blue.
const vec3 SL_BLUE = vec3(0.03, 0.16, 1.0);

varying vec3 vP;
varying vec3 vLocal;
flat varying vec4 vInfo;
flat varying vec3 vDims;
flat varying vec4 vRand;
flat varying vec3 vAccent;
flat varying vec3 vTint;
flat varying vec4 vTail;
flat varying vec3 vHead;
flat varying vec2 vStrobe;

// A filled rectangle, antialiased; it dims rather than aliases when it is thinner than a pixel.
float sl_rect(vec2 p, vec2 centre, vec2 halfSize, vec2 fw) {
  return sc_line(p.x - centre.x, halfSize.x, fw.x) * sc_line(p.y - centre.y, halfSize.y, fw.y);
}

void main() {
  // Flat shading from derivatives, taken before any branch.
  vec3 ddx = dFdx(vLocal), ddy = dFdy(vLocal);
  vec3 fw = abs(ddx) + abs(ddy);
  vec3 n = cross(ddx, ddy);
  float nl = length(n);
  vec3 an = nl > 1e-12 ? abs(n) / nl : vec3(0.0, 1.0, 0.0);

  float klass = vInfo.x, kind = vInfo.y, part = vInfo.z;
  float halfL = vDims.x, tall = vDims.y, halfW = vDims.z;
  float bike = step(2.5, klass);
  float boxy = step(0.5, klass) - bike, bus = step(1.5, klass) - bike;
  vec3 q = vLocal;
  vec2 zy = vec2(q.z, q.y), fzy = vec2(fw.z, fw.y);
  vec2 zyN = vec2(q.z / halfW, q.y / tall), fzyN = vec2(fw.z / halfW, fw.y / tall);
  float floorY = bike > 0.5 ? 0.45 : klass < 0.5 ? 0.2 : 0.3 * (bus > 0.5 ? 1.48 : 1.0);

  float pr = vRand.x;
  vec3 paint = pr < 0.30 ? vec3(0.012, 0.010, 0.030) : pr < 0.48 ? vec3(0.085, 0.008, 0.020)
    : pr < 0.62 ? vec3(0.008, 0.045, 0.060) : pr < 0.76 ? vec3(0.045, 0.010, 0.085)
    : pr < 0.88 ? vec3(0.110, 0.110, 0.130) : vec3(0.120, 0.045, 0.008);
  if (kind > 1.5) paint = vec3(0.11, 0.11, 0.13);
  else if (kind > 0.5) paint = vec3(0.14, 0.095, 0.006);
  if (bus > 0.5) paint = vec3(0.02, 0.016, 0.04) + vTint * 0.035;

  // The street's neon slides over the flanks and the glass as the car passes it.
  float streak = smoothstep(0.5, 0.85, sc_noise(vec2((vInfo.w + q.x) * 0.07, q.y * 1.7 + pr * 31.0)));
  float up = an.y;
  vec3 col = paint * (0.3 + 0.7 * up) + uSkyMid * 0.05 * up + vTint * streak * 0.16 * (1.0 - up);
  vec3 glow = vec3(0.0);
  bool cut = false;

  if (part < 0.5) {
    if (an.x > 0.9) {
      if (q.x < 0.0) {
        // Tail lights.
        float m;
        if (bike > 0.5) {
          m = sl_rect(zy, vec2(0.0, 0.8), vec2(0.075, 0.05), fzy);
        } else if (klass < 0.5) {
          float bar = sl_rect(zy, vec2(0.0, 0.66), vec2(0.84, 0.07), fzy);
          float blocks = sl_rect(vec2(abs(q.z), q.y), vec2(0.6, 0.66), vec2(0.25, 0.08), fzy);
          m = vRand.y < 0.4 ? bar : vRand.y < 0.7 ? blocks : bar * (0.25 + 0.75 * sc_pulse(q.z / 0.21 + 0.5, 0.7, fw.z / 0.21));
        } else {
          m = sl_rect(vec2(abs(zyN.x), zyN.y), vec2(0.8, 0.36), vec2(0.11, 0.07), fzyN);
          m += bus * sl_rect(zyN, vec2(0.0, 0.52), vec2(0.5, 0.012), fzyN);
        }
        glow += vTail.rgb * m * 5.5;
      } else {
        // Headlights.
        float m;
        if (bike > 0.5) {
          m = sl_rect(zy, vec2(0.0, 0.8), vec2(0.09, 0.07), fzy);
        } else if (klass < 0.5) {
          float lamps = sl_rect(vec2(abs(q.z), q.y), vec2(0.6, 0.39), vec2(0.24, 0.055), fzy);
          float bar = sl_rect(zy, vec2(0.0, 0.39), vec2(0.86, 0.03), fzy);
          m = fract(vRand.y * 7.0) < 0.3 ? bar : lamps;
        } else {
          m = sl_rect(vec2(abs(zyN.x), zyN.y), vec2(0.68, 0.25), vec2(0.17, 0.035), fzyN);
        }
        glow += vHead * m * 7.0;
      }
    } else if (an.z > 0.9) {
      // Flanks: a lit skirt, and a route stripe on vans and buses.
      glow += vAccent * sc_line(q.y - (floorY + 0.05), 0.03, fw.y) * (0.5 + vTail.a) * 1.3;
      glow += vAccent * boxy * sc_line(zyN.y - mix(0.4, 0.33, bus), mix(0.008, 0.014, bus), fzyN.y) * mix(1.2, 2.6, bus);
    } else if (q.y < floorY + 0.01) {
      col = vec3(0.004, 0.004, 0.008);
    }
  } else if (part < 1.5 && bike > 0.5) {
    // The rider: dark leathers, a helmet in the paint colour, and a lit visor round its front.
    float helmet = step(tall - 0.3, q.y);
    col = mix(vec3(0.012, 0.010, 0.022) * (0.4 + 0.6 * up), col, helmet);
    glow += vAccent * sc_line(q.y - (tall - 0.16), 0.035, fw.y) * step(-0.02, q.x) * (1.0 - up) * 3.0;
  } else if (part < 1.5) {
    float base = klass < 0.5 ? 0.7 : 1.18 * (bus > 0.5 ? 1.48 : 1.0);
    float t = clamp((q.y - base) / (tall - base), 0.0, 1.0);
    vec3 glass = vec3(0.006, 0.008, 0.02) + uSkyHorizon * 0.09 * t * t + vTint * streak * 0.22;
    if (up > 0.85) {
      // Roof: a neon roof line, a taxi sign or an emergency light bar.
      if (klass < 0.5) {
        vec2 fxz = vec2(fw.x, fw.z);
        if (kind > 1.5) {
          vec3 bar = sc_neon(6.0) * vStrobe.x * (1.0 - step(0.0, q.z)) + SL_BLUE * vStrobe.y * step(0.0, q.z);
          glow += bar * sl_rect(q.xz, vec2(-0.45, 0.0), vec2(0.1, 0.56), fxz) * 9.0;
        } else if (kind > 0.5) {
          glow += sc_neon(4.0) * sl_rect(q.xz, vec2(-0.45, 0.0), vec2(0.1, 0.3), fxz) * 3.0;
        } else {
          glow += vAccent * sc_line(q.z, 0.045, fw.z) * step(0.45, vRand.z) * 2.2;
        }
      } else {
        glow += vAccent * bus * sc_line(abs(q.z) - halfW * 0.7, 0.04, fw.z) * 1.6;
      }
    } else if (klass < 0.5) {
      col = glass;
    } else if (bus > 0.5) {
      if (an.z > 0.9) {
        // A lit saloon behind a row of windows.
        float band = sc_line(zyN.y - 0.74, 0.115, fzyN.y);
        float win = sc_pulse(q.x / 1.5 + 0.25, 0.86, fw.x / 1.5);
        vec3 interior = mix(vec3(1.0, 0.74, 0.42), vec3(0.5, 0.9, 1.0), step(0.55, vRand.w));
        col = paint * 0.5;
        glow += interior * band * win * mix(0.7, 1.1, t);
      } else {
        col = glass;
        // Destination sign over the windscreen.
        if (q.x > 0.0) glow += sc_neon(3.0) * sl_rect(zyN, vec2(0.0, 0.93), vec2(0.6, 0.028), fzyN) * 2.5;
      }
    } else {
      // Van: a windscreen and a door window; the rest is panel.
      float door = step(0.9, an.z) * sl_rect(vec2(q.x, zyN.y), vec2(1.55, 0.76), vec2(0.5, 0.12), vec2(fw.x, fzyN.y));
      col = an.x > 0.3 && q.x > 0.0 ? glass : mix(col, glass, door);
    }
  } else {
    col = vec3(0.004, 0.004, 0.008);
    if (an.z > 0.9) {
      // The axle is a box; cut its two outer faces down to round wheels (a pair of them on a bus).
      float sx = klass < 0.5 ? halfL / 2.25 : (bus > 0.5 ? 2.13 : 1.0);
      float sy = bus > 0.5 ? 1.48 : 1.0;
      float r0 = bike > 0.5 ? 0.33 : klass < 0.5 ? 0.34 : 0.36;
      float axle = bike > 0.5 ? (q.x > 0.0 ? 0.72 : -0.7) : (klass < 0.5 ? (q.x > 0.0 ? 1.42 : -1.38) : (q.x > 0.0 ? 1.75 : -1.75)) * sx;
      float radius = r0 * sy;
      float r = length(vec2(max(abs(q.x - axle) - max(r0 * sx - radius, 0.0), 0.0), q.y - radius));
      cut = r > radius;
      // A dull hub, and on a few cars a lit ring.
      col += paint * 0.35 * (1.0 - step(0.55 * radius, r));
      glow += vAccent * sc_line(r - 0.68 * radius, 0.03, max(fw.x, fw.y)) * step(0.8, vRand.z) * (1.0 - boxy) * 2.0;
    }
  }

  // Every derivative was taken above, so discarding here cannot outline anything.
  if (cut) discard;
  col += glow * uGlow;
  gl_FragColor = vec4(sc_fog(col, vP), 1.0);
}
