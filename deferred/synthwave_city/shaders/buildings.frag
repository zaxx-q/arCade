// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/buildings/), adapted to GLSL 3.30 only by the prelude files.
#define SB_PODIUM 0.0
#define SB_SHAFT 1.0
#define SB_GLASS 2.0
#define SB_SLAB 3.0
#define SB_STRIP 4.0
#define SB_CROWN 5.0
#define SB_SPIRE 6.0
#define SB_MEGA 7.0
#define SB_KIOSK 8.0
#define SB_FLAG_ROOF 1.0
#define SB_FLAG_STREET 2.0
#define SB_FLAG_ROUND 4.0
#define SB_FLAG_LANDMARK 8.0
#define SB_FACE_TOP 3.5
// Added to the panel kind when the panel is the whole facade and its "pixels" are the windows.
#define SB_WHOLE 16.0

// xyz: render-space position. w: u on the face, metres, left to right seen from outside.
varying vec4 vPu;
// xyz: normal. w: v on the face, metres: height above the box base (on a roof: distance along z).
varying vec4 vNv;
// Face width, face height, face id (0..3 sides, 4 top), box flags.
flat varying vec4 vFace;
// Face seed (an exact integer), building seed (an exact integer), style, base y of the box.
flat varying vec4 vSeed;
// Window cell width and height in metres, glazed share of the cell in u and in v.
flat varying vec4 vGrid;
// Lit share of the windows, zone size in cells (u, v), share of windows in the accent colour.
flat varying vec4 vLit;
// Spacing of light bands (m, 0 = none), spacing of fins (m, 0 = none), trim half-width (m), trim gain.
flat varying vec4 vDeco;
// Sign panel rectangle on the face: u0, v0, width, height in metres (width 0 = none).
flat varying vec4 vPanel;
// Panel kind (+ SB_WHOLE), LED pitch in u and v (m), seed (an exact integer).
flat varying vec4 vPanelK;
// Shop front: height (m, 0 = none), bay width (m), seed (an exact integer), height of the sign strip (m).
flat varying vec4 vShop;
// Window colours on the light scale. vC1.w: roof height of the whole building. vC2.w: variant 0..1.
flat varying vec4 vC1;
flat varying vec4 vC2;
// Trim colours, luminance-balanced. w: palette indices of the two panel colours.
flat varying vec4 vTrimA;
flat varying vec4 vTrimB;

float sb_bit(float flags, float bit) { return mod(floor(flags / bit), 2.0); }
bool sb_is(float style, float which) { return abs(style - which) < 0.5; }
// Palette colours differ five-fold in luminance (violet 0.2, cyan 0.7), and bloom keys on luminance.
// This evens them out most of the way, so a magenta tube glows like a cyan one.
vec3 sb_even(vec3 c, float power) { return c / pow(max(sc_luma(c), 0.05), power); }

// A neon tube: sc_line with a floor under the dimming, so far outlines stay drawn instead of vanishing.
// Seen close, where the tube is several pixels wide, it has a hot core and dimmer shoulders.
float sb_tube(float d, float w, float fw) {
  fw = max(fw, 1e-5);
  float ww = max(w, 0.75 * fw);
  float x = min(abs(d) / ww, 1.0);
  float body = mix(1.0, 0.5 + 0.85 * (1.0 - x * x), clamp(0.5 * w / fw - 0.5, 0.0, 1.0));
  return (1.0 - smoothstep(ww - fw, ww + fw, abs(d))) * body * max(w / ww, 0.3);
}

// One pseudo-glyph pixel: 5 x 7 dots, mirrored left to right so it reads as a character.
// cell: LED index, already scrolled. id: an exact number naming the line of text.
float sb_glyph(vec2 cell, float id, float seed) {
  vec2 gid = floor(cell / vec2(6.0, 9.0));
  vec2 sub = cell - gid * vec2(6.0, 9.0);
  float blank = step(0.2, sc_hash31(vec3(gid.x, gid.y + 64.0 * id, seed)));
  float sx = min(sub.x, 4.0 - sub.x);
  float bit = step(0.45, sc_hash31(vec3(sx + 3.0 * sub.y, gid.x + 64.0 * gid.y + 4096.0 * id, seed)));
  return bit * blank * step(sub.x, 4.5) * step(0.5, sub.y) * step(sub.y, 7.5);
}

// The panel at this fragment. cover: how much of the pixel the panel takes (0 outside).
vec3 sb_panel(vec2 uv, vec2 fw, out float cover) {
  vec2 pm = uv - vPanel.xy;
  vec2 ps = vPanel.zw;
  vec2 e2 = min(pm, ps - pm);
  vec2 cv = clamp(e2 / fw + 0.5, 0.0, 1.0);
  cover = cv.x * cv.y;

  float kind = vPanelK.x;
  float whole = step(SB_WHOLE - 0.5, kind);
  kind -= whole * SB_WHOLE;
  vec2 pitch = vPanelK.yz;
  float seed = vPanelK.w;
  vec2 n = max(vec2(1.0), floor(ps / pitch + 0.5));
  vec2 q = pm / pitch;
  vec2 qi = clamp(floor(q), vec2(0.0), n - 1.0);
  vec2 c01 = (qi + 0.5) / n;
  vec3 hs = sc_hash33(vec3(seed, 71.0, 9.0));
  float t = uTime;
  float pace = 0.3 + 0.7 * uMotion;

  vec3 ca = sb_even(sc_neon(vTrimA.w), 0.75);
  vec3 cb = sb_even(sc_neon(vTrimB.w), 0.75);
  float I = 0.0, tone = 0.0, avg = 0.4, hot = 0.0;

  if (kind < 0.5) {
    // Chevrons marching up or down.
    float per = 6.0 + floor(hs.x * 6.0);
    float ax = abs(qi.x + 0.5 - 0.5 * n.x) * (pitch.x / pitch.y);
    float ph = (qi.y + 0.5 + ax * (hs.z < 0.5 ? 1.0 : -1.0)) / per + t * 0.7 * pace * (hs.y < 0.5 ? 1.0 : -1.0);
    I = step(fract(ph), 0.5);
    tone = mod(floor(ph), 2.0);
    avg = 0.5;
  } else if (kind < 1.5) {
    // Equaliser: bars jump on the beat and fall back.
    float barW = max(1.0, floor(n.x / 12.0 + 0.5));
    float b = floor(qi.x / barW);
    float gap = (whole < 0.5 && barW > 2.5) ? step(mod(qi.x, barW), barW - 1.5) : 1.0;
    float beat = mix(0.6, uBeat.x, uMotion);
    float lv = (0.18 + 0.82 * sc_hash31(vec3(b, uBeat.z, seed))) * (0.42 + 0.58 * beat);
    float rows = floor(lv * n.y + 0.5);
    I = step(qi.y + 0.5, rows) * gap;
    hot = step(rows - 1.5, qi.y) * I;
    tone = c01.y / max(lv, 0.05) * 0.8;
    avg = 0.33;
  } else if (kind < 2.5) {
    // Rings, squares or diamonds growing out of the centre.
    vec2 dl = (qi + 0.5 - 0.5 * n) * vec2(pitch.x / pitch.y, 1.0);
    float r = hs.x < 0.4 ? length(dl) : hs.x < 0.7 ? max(abs(dl.x), abs(dl.y)) : abs(dl.x) + abs(dl.y);
    float ph = r / (5.0 + floor(hs.z * 5.0)) - t * 0.55 * pace;
    I = step(fract(ph), 0.45);
    tone = mod(floor(ph), 2.0);
    avg = 0.45;
  } else if (kind < 3.5) {
    // Wipes: stacked bars sliding against each other.
    float bands = 3.0 + floor(hs.x * 4.0);
    float rb = floor(c01.y * bands);
    float hr = sc_hash31(vec3(rb, 5.0, seed));
    float dir = mod(rb, 2.0) * 2.0 - 1.0;
    float ph = c01.x * (0.6 + hr) + dir * t * (0.1 + 0.22 * hr) * pace + hr * 7.0;
    I = step(fract(ph), 0.5);
    tone = mod(rb + floor(ph), 2.0);
    avg = 0.5;
  } else if (kind < 4.5) {
    // Checker shimmer.
    float cs = whole > 0.5 ? 1.0 : 2.0 + floor(hs.x * 3.0);
    vec2 cc = floor(qi / cs);
    vec3 hk = sc_hash33(vec3(cc, seed));
    float flick = fract(hk.x + 0.618 * floor(t * (0.5 + 1.5 * hk.y) * pace + hk.z * 5.0));
    I = 0.1 + 0.9 * step(0.45, flick);
    tone = mod(cc.x + cc.y, 2.0);
    avg = 0.6;
  } else if (kind < 5.5) {
    // Rain of light: a bright head falling down each column with a tail above it.
    vec3 hk = sc_hash33(vec3(qi.x, 13.0, seed));
    float len = max(6.0, n.y * (0.35 + 0.6 * hk.z));
    float ph = fract((qi.y + 0.5) / len + t * (7.0 + 16.0 * hk.x) * pace / len + hk.y);
    ph = 1.0 - ph;
    I = ph * ph * ph;
    hot = step(0.93, ph);
    tone = step(0.7, hk.z);
    avg = 0.25;
  } else if (kind < 6.5) {
    // The striped sun.
    vec2 dm = (qi + 0.5) * pitch - 0.5 * ps;
    float R = 0.43 * min(ps.x, ps.y);
    float y01 = clamp(0.5 + 0.5 * dm.y / R, 0.0, 1.0);
    float cut = step(fract(y01 * 11.0 - t * 0.18 * pace), mix(0.3, 0.92, clamp(y01 / 0.56, 0.0, 1.0)));
    I = step(length(dm), R) * mix(cut, 1.0, step(0.56, y01));
    tone = y01;
    ca = sb_even(uPalette[0], 0.75);
    cb = sb_even(uPalette[4], 0.75);
    avg = 0.3;
  } else {
    // Lines of invented glyphs scrolling sideways.
    float line = floor(qi.y / 9.0);
    float hl = sc_hash31(vec3(line, 17.0, seed));
    float scroll = floor(t * (5.0 + 9.0 * hl) * pace);
    I = sb_glyph(vec2(qi.x + scroll, qi.y), line, seed);
    tone = mod(line, 2.0);
    avg = 0.25;
  }

  // The LED grid, box-filtered; the pattern gives way to its average once LEDs are sub-pixel.
  vec2 gf = fw / pitch;
  vec2 ld = whole > 0.5 ? vGrid.zw : vec2(0.78);
  float led = sc_pulse(q.x - 0.5 * (1.0 - ld.x), ld.x, gf.x) * sc_pulse(q.y - 0.5 * (1.0 - ld.y), ld.y, gf.y);
  float detail = sc_detail(1.0, max(gf.x, gf.y) * 0.4);
  I = mix(avg, I, detail);
  hot *= detail;
  vec3 pc = mix(ca, cb, clamp(tone, 0.0, 1.0));
  pc = mix(pc, vec3(1.6), hot * 0.7);
  // A framed sign keeps a dark border of one LED inside its frame.
  float inner = whole > 0.5 ? 1.0 : step(0.5, qi.x) * step(qi.x, n.x - 1.5) * step(0.5, qi.y) * step(qi.y, n.y - 1.5);
  vec3 col = vec3(0.012, 0.008, 0.03) + pc * (led * inner * (0.03 + I * 1.45 * uGlow));
  if (whole < 0.5) {
    float e = min(e2.x, e2.y);
    col += cb * (sb_tube(e - 0.3, 0.16, max(fw.x, fw.y)) * 1.7 * uGlow);
  }
  return col;
}

// Interior mapping: what a ray through a window hits inside a box-shaped room. No geometry, no loop.
// f: where the ray enters, 0..1 across and up the room front. room: width, height, depth in metres.
// vt: view direction in face space (x right, y up, z into the wall, z > 0).
// Returns the hit point in room units (z = 1 is the back wall) and which surface it is:
// 0 back wall, 1 ceiling, 2 floor, 3 side wall.
vec4 sb_roomHit(vec2 f, vec3 room, vec3 vt) {
  vec3 rd = vt / room;
  rd = vec3(abs(rd.x) < 1e-5 ? 1e-5 : rd.x, abs(rd.y) < 1e-5 ? 1e-5 : rd.y, max(rd.z, 1e-5));
  vec3 ro = vec3(clamp(f, 0.0, 1.0), 0.0);
  vec3 tt = (step(vec3(0.0), rd) - ro) / rd;
  float t = min(tt.x, min(tt.y, tt.z));
  float surface = t == tt.z ? 0.0 : t == tt.y ? (rd.y > 0.0 ? 1.0 : 2.0) : 3.0;
  return vec4(ro + rd * t, surface);
}

// Brightness of an office or a flat seen through its window, about 0.85 on average:
// a lit ceiling with strips, a dim floor, walls falling off with depth. h: a random 0..1.
float sb_room(vec2 f, vec3 room, vec3 vt, float h) {
  vec4 hit = sb_roomHit(f, room, vt);
  vec3 hp = hit.xyz;
  float s;
  if (hit.w < 0.5) {
    // Back wall, with a bright doorway on some.
    float door = step(abs(hp.x - 0.3 - 0.4 * h), 0.14) * step(hp.y, 0.72);
    s = 0.6 + 0.3 * hp.y + 0.55 * door * step(h, 0.6);
  } else if (hit.w < 1.5) {
    s = 1.0 + 0.8 * step(fract(hp.z * 2.5 + h), 0.45);
  } else if (hit.w < 2.5) {
    s = 0.3;
  } else {
    s = (0.42 + 0.3 * hp.y) * (0.75 + 0.5 * step(fract(hp.z * 2.0 + h), 0.5));
  }
  return s * mix(1.2, 0.5, hp.z);
}

// A shop seen through its window: ceiling strips, a floor that picks up the light, and a back wall
// of posters and shelves in loud colours.
vec3 sb_shopRoom(vec2 f, vec3 room, vec3 vt, vec3 light, float bay, float seed) {
  vec4 hit = sb_roomHit(f, room, vt);
  vec3 hp = hit.xyz;
  vec3 c;
  if (hit.w < 0.5) {
    vec2 dg = hp.xy * vec2(max(2.0, floor(room.x / 1.5)), 3.0);
    vec2 di = floor(dg);
    vec2 df = dg - di;
    vec3 hd = sc_hash33(vec3(di.x + 16.0 * di.y, bay, seed));
    float inside = step(0.1, df.x) * step(df.x, 0.9) * step(0.12, df.y) * step(df.y, 0.88);
    vec3 poster = sb_even(sc_neon(floor(hd.x * 7.0)), 0.5) * (0.3 + 0.9 * hd.y);
    c = mix(light * 0.3, poster, inside * step(0.3, hd.z));
  } else if (hit.w < 1.5) {
    c = light * (0.75 + 1.1 * step(fract(hp.z * 3.0 + 0.2), 0.4));
  } else if (hit.w < 2.5) {
    c = light * (0.22 + 0.25 * (1.0 - hp.z));
  } else {
    // Side walls: shelves of goods in mixed colours.
    vec2 sg = vec2(hp.z * max(2.0, floor(room.z / 1.2)), hp.y * 4.0);
    vec2 si = floor(sg);
    vec2 sf = sg - si;
    vec3 hd = sc_hash33(vec3(si.x + 16.0 * si.y, bay + 0.5, seed));
    float goods = step(0.12, sf.x) * step(sf.x, 0.88) * step(0.18, sf.y) * step(sf.y, 0.3 + 0.6 * hd.y);
    vec3 item = sb_even(sc_neon(floor(hd.x * 7.0)), 0.5) * (0.35 + 0.6 * hd.z);
    c = mix(light * (0.28 + 0.3 * hp.y), item, goods * step(hp.y, 0.75));
  }
  return c * mix(1.25, 0.6, hp.z);
}

// Distance to the outline of an equilateral triangle of "radius" r, point up.
float sb_triangle(vec2 p, float r) {
  const float k = 1.7320508;
  p.x = abs(p.x) - r;
  p.y = p.y + r / k;
  if (p.x + k * p.y > 0.0) p = vec2(p.x - k * p.y, -k * p.x - p.y) * 0.5;
  p.x -= clamp(p.x, -2.0 * r, 0.0);
  return length(p);
}

// The shop front along the bottom of a street-level box. cover: share of the pixel under the band.
vec3 sb_shop(vec2 uv, vec2 fw, vec3 vt, out float cover) {
  float hS = vShop.x, bayW = vShop.y, seed = vShop.z, signH = vShop.w;
  cover = clamp((hS - uv.y) / fw.y + 0.5, 0.0, 1.0);
  float fwm = max(fw.x, fw.y);
  float bay = floor(uv.x / bayW);
  float bu = uv.x - bay * bayW;
  vec3 h1 = sc_hash33(vec3(bay, 3.0, seed));
  vec3 h2 = sc_hash33(vec3(bay, 9.0, seed));
  float i1 = floor(h1.x * 7.0);
  float i2 = mod(i1 + 1.0 + floor(h1.y * 5.0), 7.0);
  vec3 c1 = sb_even(sc_neon(i1), 0.5);
  vec3 c2 = sb_even(sc_neon(i2), 0.75);
  float near = sc_detail(bayW, fwm * 9.0);

  // Glass between piers, from a low plinth up to the sign. A third of the shops are lit white,
  // the rest soak in one colour.
  float pier = 0.3;
  float glassTop = max(hS - signH - 0.6, 1.0);
  float gx = sc_pulse((uv.x - pier) / bayW, 1.0 - 2.0 * pier / bayW, fw.x / bayW);
  float gy = clamp((uv.y - 0.45) / fw.y + 0.5, 0.0, 1.0) * clamp((glassTop - uv.y) / fw.y + 0.5, 0.0, 1.0);
  vec3 light = h1.z < 0.33 ? vec3(1.0, 0.86, 0.68) * (0.9 + 0.25 * h2.y) : mix(vec3(1.0, 0.82, 0.6), c1, 0.88) * (0.6 + 0.4 * h2.y);
  vec3 inside = light * 0.8;
  float shape = 0.0;
  if (near > 0.01) {
    inside = sb_shopRoom(vec2(bu / bayW, uv.y / glassTop), vec3(bayW, glassTop, 7.0), vt, light, bay, seed);
    // The window display: goods standing behind the glass, seen as dark shapes against the light.
    float column = floor(bu / 0.55);
    float tall = sc_hash21(vec2(column, seed + bay));
    float goods = step(uv.y - 0.45, (0.08 + 0.3 * tall * tall) * glassTop) * step(0.25, tall) * step(fract(bu / 0.55), 0.82);
    inside *= 1.0 - 0.8 * goods;
    // Glazing bars and a transom.
    float bars = 2.0 + floor(h2.z * 3.0);
    float bx = (fract(bu / bayW * bars + 0.5) - 0.5) * bayW / bars;
    float frame = max(sc_line(bx, 0.05, fw.x), sc_line(uv.y - 0.72 * glassTop, 0.05, fw.y));
    inside = mix(light * 0.8, inside * (1.0 - 0.85 * frame), near);
    // A neon shape hung in the window: ring, triangle or diamond.
    vec2 sp = vec2(bu - 0.5 * bayW, uv.y - 0.42 * glassTop);
    float r = 0.24 * min(bayW, glassTop);
    float d = h2.x < 0.2 ? abs(length(sp) - r) : h2.x < 0.38 ? sb_triangle(sp, r) : h2.x < 0.52 ? abs(abs(sp.x) + abs(sp.y) - r) : 1e3;
    shape = sb_tube(d, 0.055, fwm) * near;
  }
  vec3 col = vec3(0.016, 0.01, 0.04);
  col = mix(col, inside, gx * gy);
  col += c2 * (shape * gx * gy * 2.4 * uGlow);

  // The sign strip above the glass: glyphs, tubes, a light box, or a chase of bulbs.
  float sy0 = glassTop + 0.3, sy1 = hS - 0.3;
  float sh = max(sy1 - sy0, 0.2);
  float margin = 0.5 + 1.2 * h2.z;
  float sgy = clamp((uv.y - sy0) / fw.y + 0.5, 0.0, 1.0) * clamp((sy1 - uv.y) / fw.y + 0.5, 0.0, 1.0);
  float sgx = sc_pulse((uv.x - margin) / bayW, 1.0 - 2.0 * margin / bayW, fw.x / bayW);
  float pitch = sh / 9.0;
  vec2 q = vec2(bu - margin, uv.y - sy0) / pitch;
  vec2 gf = fw / pitch;
  float fine = sc_detail(1.0, max(gf.x, gf.y) * 0.5);
  float amount;
  if (h1.y < 0.4) {
    float led = sc_pulse(q.x - 0.1, 0.8, gf.x) * sc_pulse(q.y - 0.1, 0.8, gf.y);
    float glyph = sb_glyph(vec2(floor(q.x), clamp(floor(q.y), 0.0, 8.0)), bay, seed);
    amount = mix(0.3, glyph * led, fine);
  } else if (h1.y < 0.6) {
    float d = uv.y - 0.5 * (sy0 + sy1);
    amount = max(sc_line(d - 0.25 * sh, 0.1 * sh, fw.y), sc_line(d + 0.25 * sh, 0.1 * sh, fw.y));
  } else if (h1.y < 0.8) {
    // Light box: a bright plate with the glyphs cut out of it.
    float glyph = sb_glyph(vec2(floor(q.x * 0.75), clamp(floor(q.y), 0.0, 8.0)), bay, seed);
    amount = 0.62 * (1.0 - 0.9 * glyph * fine);
  } else {
    // Theatre bulbs chasing round a dark plate.
    float step3 = floor(uTime * 4.0 * (0.3 + 0.7 * uMotion));
    float bulbs = step(mod(floor(q.x * 0.5) + step3, 3.0), 0.5) * sc_pulse(q.x * 0.5 - 0.15, 0.7, gf.x * 0.5);
    float rows = max(sc_line(q.y - 1.5, 0.8, gf.y), sc_line(q.y - 7.5, 0.8, gf.y));
    amount = mix(0.25, bulbs * rows + 0.5 * sc_line(q.y - 4.5, 0.5, gf.y), fine);
  }
  float blink = 1.0;
  if (h1.z > 0.8) blink = mix(0.8, 0.3 + 0.7 * step(0.3, fract(uTime * (0.6 + h2.z) + h2.y * 9.0)), uMotion);
  col += c2 * (amount * sgx * sgy * 2.1 * blink * uGlow);
  // The sign lights the fascia round it.
  col += c2 * (0.05 * sgx * clamp((uv.y - glassTop) / 0.6, 0.0, 1.0));
  return col;
}

// Unlit surface colour: deep indigo, never grey, lifted a little by the mood's ambient light.
vec3 sb_wall() { return vec3(0.011, 0.007, 0.032) + uAmbient * 0.45; }

// The top face. uv is metres from one corner; W and H are the footprint.
vec3 sb_roof(vec2 uv, vec2 fw) {
  float W = vFace.x, H = vFace.y;
  float seed = vSeed.x;
  float fwm = max(fw.x, fw.y);
  vec2 c = uv - 0.5 * vec2(W, H);
  float isRound = sb_bit(vFace.w, SB_FLAG_ROUND);
  float isRoof = sb_bit(vFace.w, SB_FLAG_ROOF);
  float e = isRound > 0.5 ? 0.5 * W - length(c) : min(min(uv.x, W - uv.x), min(uv.y, H - uv.y));
  vec3 col = sb_wall() * 1.25;
  if (sb_is(vSeed.z, SB_SPIRE)) return vTrimA.rgb * 1.6 * uGlow;

  // Parapet: a thin outline and its spill on the roof deck.
  float tw = min(vDeco.z, 0.4);
  col += vTrimA.rgb * (sb_tube(e - tw - 0.15, tw * 0.8, fwm) * vDeco.w * 0.7 * uGlow);
  col += vTrimA.rgb * (0.05 * exp(-max(e, 0.0) * 0.7));

  // Plant: a 7 m grid of cooling units, vents and a few small lights, away from the edge.
  vec2 pg = uv / 7.0;
  vec2 pid = floor(pg);
  vec2 pf = pg - pid;
  vec3 hp = sc_hash33(vec3(pid, seed));
  float inside = smoothstep(3.0, 4.0, e);
  float near = sc_detail(7.0, fwm * 3.0);
  vec2 bh = 0.22 + 0.2 * hp.yz;
  vec2 bd = abs(pf - 0.5) - bh;
  float unit = step(hp.x, 0.34) * (1.0 - smoothstep(-0.02, 0.02, max(bd.x, bd.y)));
  float vent = step(fract(pf.y * 9.0), 0.45) * step(0.5, hp.z);
  col += inside * near * unit * (sb_wall() * (1.2 + 1.4 * vent) + vTrimB.rgb * 0.012);
  float lamp = step(0.8, hp.x) * (1.0 - smoothstep(0.03, 0.07, length(pf - 0.3 - 0.4 * hp.yz)));
  vec3 lampCol = hp.y < 0.5 ? vec3(1.0, 0.85, 0.6) : vTrimB.rgb;
  col += inside * lampCol * mix(0.012, lamp * 2.5, sc_detail(1.2, fwm)) * uGlow;

  // Helipad on some of the larger exposed roofs.
  float hpad = sc_hash11(seed + 3.0);
  if (isRoof > 0.5 && min(W, H) > 30.0 && hpad < 0.4) {
    float r = length(c);
    float ring = sc_line(r - 8.0, 0.3, fwm);
    vec2 a = abs(c);
    float bar = max(step(abs(a.x - 2.4), 0.45) * step(a.y, 3.6), step(a.y, 0.45) * step(a.x, 2.4));
    bar *= sc_detail(1.0, fwm);
    vec3 padCol = hpad < 0.2 ? vec3(1.0, 0.8, 0.25) : vTrimB.rgb;
    col = mix(col, sb_wall() * 2.2, (1.0 - smoothstep(8.0, 8.3, r)) * 0.7);
    col += padCol * ((ring * 1.5 + bar * 1.2) * uGlow);
  }
  return col;
}

// The lit cap of a tower: louvres, bars or a chase of segments, glowing in the main colour.
vec3 sb_crown(vec2 uv, vec2 fw) {
  float W = vFace.x, H = vFace.y;
  float variant = vC2.w;
  float mega = sb_bit(vFace.w, SB_FLAG_LANDMARK);
  float isRound = sb_bit(vFace.w, SB_FLAG_ROUND);
  float g = uv.y / H;
  float beat = 1.0 + 0.5 * uBeat.x * uMotion * step(0.5, fract(variant * 5.0));
  float pattern;
  if (variant < 0.4) {
    float s = mix(0.9, 2.0, fract(variant * 13.0));
    pattern = sc_pulse(uv.x / s, 0.55, fw.x / s) * mix(1.25, 0.5, g);
  } else if (variant < 0.7) {
    float s = H / max(2.0, floor(H / 1.4 + 0.5));
    pattern = sc_pulse(uv.y / s + 0.2, 0.5, fw.y / s) * 1.1;
  } else {
    float s = mix(5.0, 10.0, fract(variant * 13.0));
    float run = sc_pulse(uv.x / s - uTime * 0.35 * (0.3 + 0.7 * uMotion), 0.6, fw.x / s);
    pattern = (0.35 + 0.9 * run) * (0.6 + 0.4 * sc_pulse(g * 2.0 + 0.25, 0.5, fw.y / H * 2.0));
  }
  vec3 col = sb_wall() + vTrimA.rgb * (pattern * (0.85 + 0.5 * mega) * beat * uGlow);
  float tw = vDeco.z;
  float eu = min(uv.x, W - uv.x);
  float tube = max(sb_tube(H - uv.y - tw, tw, fw.y), sb_tube(uv.y - tw, tw, fw.y));
  tube = max(tube, (1.0 - isRound) * sb_tube(eu - tw, tw, fw.x));
  col += mix(vTrimB.rgb, vec3(1.5), 0.25 + 0.4 * mega) * (tube * vDeco.w * uGlow);
  return col;
}

// A thin lit mast: a bright core, rings up its length and a pulse climbing it.
vec3 sb_spire(vec2 uv, vec2 fw) {
  float W = vFace.x, H = vFace.y;
  float mega = sb_bit(vFace.w, SB_FLAG_LANDMARK);
  float core = sc_line(uv.x - 0.5 * W, 0.2 * W, fw.x);
  float s = 7.0 + 5.0 * mega;
  float ring = sc_line((fract(uv.y / s + 0.5) - 0.5) * s, 0.35, fw.y) * sc_detail(s, fw.y * 3.0);
  float climb = fract(uv.y / (60.0 + 60.0 * mega) - uTime * 0.25);
  float pulse = climb * climb * climb * climb;
  vec3 col = sb_wall() * 2.0;
  col += vTrimA.rgb * (core * (0.8 + 1.6 * pulse) * uGlow);
  col += mix(vTrimB.rgb, vec3(1.4), 0.5) * (ring * 2.0 * uGlow);
  col += vTrimA.rgb * (0.25 * (1.0 - uv.y / H));
  return col;
}

// What a pane of glass reflects: the sky in the mirror direction, pinker towards the sun.
vec3 sb_sky(vec3 r) {
  vec2 rh = r.xz / max(length(r.xz), 1e-4);
  vec2 sh = uSunDir.xz / max(length(uSunDir.xz), 1e-4);
  float toSun = 0.5 + 0.5 * dot(rh, sh);
  vec3 low = mix(uSkyMid * 0.7, uSkyHorizon, toSun * toSun);
  vec3 sky = mix(low, uSkyMid, smoothstep(0.03, 0.4, r.y));
  sky = mix(sky, uSkyTop, smoothstep(0.4, 0.95, r.y));
  return mix(uFogCool * 0.6, sky, smoothstep(-0.25, 0.0, r.y));
}

// A side face of an ordinary box: wall, windows, sign panel, shop front, then the neon structure.
vec3 sb_facade(vec2 uv, vec2 fw, vec3 vn, vec3 n) {
  float W = vFace.x, H = vFace.y;
  float style = vSeed.z;
  float seed = vSeed.x;
  float yAbs = vSeed.w + uv.y;
  float hn = clamp(yAbs / vC1.w, 0.0, 1.0);
  float isRound = sb_bit(vFace.w, SB_FLAG_ROUND);
  bool isMega = sb_is(style, SB_MEGA);
  float glassy = (sb_is(style, SB_GLASS) || sb_is(style, SB_KIOSK)) ? 1.0 : 0.5;

  // View direction in face space: x right, y up, z into the wall.
  vec3 tangent = vec3(n.z, 0.0, -n.x);
  vec3 vt = vec3(dot(vn, tangent), vn.y, max(-dot(vn, n), 1e-3));

  // ---- wall, washed by the glow of the street near the ground
  vec3 wall = sb_wall();
  wall += (vTrimA.rgb + vTrimB.rgb) * (0.002 + 0.012 * exp(-yAbs / 22.0));

  // ---- windows
  vec2 cell = vGrid.xy;
  vec2 duty = vGrid.zw;
  vec2 g = uv / cell;
  vec2 gf = fw / cell;
  vec2 id = floor(g);
  vec2 m0 = 0.5 * (1.0 - duty);
  float mask = sc_pulse(g.x - m0.x, duty.x, gf.x) * sc_pulse(g.y - m0.y, duty.y, gf.y);
  float dx = sc_detail(1.0, gf.x), dy = sc_detail(1.0, gf.y);
  float dd = dx * dy;

  float hz = sc_hash31(vec3(floor(id / vLit.yz), seed));
  float hr = sc_hash31(vec3(id.y, 7.0, seed));
  vec3 hw = sc_hash33(vec3(id, seed));
  // Zones are dark, ordinary or busy; a few rows are lit end to end.
  float pZone = clamp(vLit.x * (0.25 + 1.5 * smoothstep(0.15, 0.9, hz)), 0.0, 1.0);
  float rowAll = step(0.93, hr);
  float pRow = mix(clamp(pZone * (0.5 + hr), 0.0, 1.0), 1.0, rowAll);
  // A few windows re-roll every so often, each on its own clock.
  float roll = floor(uTime * 0.06 + hw.y * 11.0) * step(hw.z, 0.14);
  float lit = step(1.0 - pRow, fract(hw.x + 0.618 * roll));
  float amount = mix(pZone + 0.07 * (1.0 - pZone), mix(pRow, lit, dx), dy);
  float share = mix(vLit.w, 0.92, step(0.74, hz));
  // Curtain glass fades from one colour at the street to the other at the roof.
  if (glassy > 0.75) share = clamp(hn * 1.1 - 0.05 + (hz - 0.5) * 0.3, 0.04, 0.96);
  vec3 mean = mix(vC1.rgb, vC2.rgb, share) * 0.75;
  vec3 pane = mix(vC1.rgb, vC2.rgb, max(step(1.0 - share, hw.y), rowAll));
  // A dim window goes deeper in hue, not greyer.
  pane = mix(pane * pane / max(max(pane.r, pane.g), max(pane.b, 0.05)), pane, hw.z) * (0.45 + 0.55 * hw.z);
  float room = 0.85;
  if (dd > 0.01) {
    float floors = max(1.0, floor(cell.y / 3.8 + 0.5));
    vec2 rf = vec2(g.x - id.x, fract(g.y * floors));
    room = sb_room(rf, vec3(cell.x, cell.y / floors, 4.5), vt, hw.z);
    // Glazing bars split a wide pane, and floor slabs cross a tall one.
    vec2 paneSize = cell * duty;
    vec2 parts = max(vec2(1.0), floor(paneSize / vec2(1.7, 3.8) + 0.5));
    vec2 w = clamp((g - id - m0) / duty, 0.0, 1.0);
    vec2 bar = (fract(w * parts + 0.5) - 0.5) * paneSize / parts;
    float bars = max(sc_line(bar.x, 0.05, fw.x) * step(1.5, parts.x), sc_line(bar.y, 0.12, fw.y) * step(1.5, parts.y));
    room = mix(0.85, room * (1.0 - 0.85 * bars), dd);
  }
  vec3 glow = mix(mean, pane, dd) * room;

  // Unlit glass shows the sky; curtain glass also carries a faint gradient up the tower.
  float fresnel = 0.05 + 0.95 * pow(1.0 - clamp(vt.z, 0.0, 1.0), 4.0);
  vec3 glass = wall * 0.45 + sb_sky(reflect(vn, n)) * (min(fresnel, 0.45) * 0.3 * glassy);
  glass += mix(vTrimA.rgb, vTrimB.rgb, hn) * (0.02 * glassy);
  vec3 col = mix(wall, glass, mask) + glow * (mask * amount);
  // The reveal: a narrow rim round each window that catches its light, so the glass sits in the wall.
  vec2 rim = min(vec2(0.13) / cell, m0);
  float outer = sc_pulse(g.x - m0.x + rim.x, duty.x + 2.0 * rim.x, gf.x) * sc_pulse(g.y - m0.y + rim.y, duty.y + 2.0 * rim.y, gf.y);
  col += max(outer - mask, 0.0) * (wall * 1.2 + mix(mean, pane, dd) * (amount * 0.32));

  // ---- sign panel and shop front replace the windows where they sit
  float clear = 1.0;
  if (vPanel.z > 0.0) {
    float cover;
    vec3 signCol = sb_panel(uv, fw, cover);
    col = mix(col, signCol, cover);
    clear *= 1.0 - cover;
  }
  if (vShop.x > 0.0) {
    float cover;
    vec3 shop = sb_shop(uv, fw, vt, cover);
    col = mix(col, shop, cover);
    clear *= 1.0 - cover;
  }

  // ---- neon structure
  float tw = vDeco.z;
  float gain = vDeco.w * uGlow;
  float eu = min(uv.x, W - uv.x);
  float et = H - uv.y;
  float corner = 1.0 - isRound;
  float tube = max(sb_tube(et - tw, tw, fw.y), corner * sb_tube(eu - tw, tw, fw.x));
  col += vTrimA.rgb * (tube * gain);
  col += vTrimA.rgb * (0.05 * (exp(-et * 0.7) + corner * exp(-eu * 0.7)));

  if (vShop.x > 0.0) {
    // A cornice tube along the top of the shops, the whole length of the block.
    col += vTrimB.rgb * (sb_tube(uv.y - vShop.x, 0.09, fw.y) * 0.8 * gain);
    // The signs wash the wall above them.
    col += vTrimB.rgb * (0.04 * exp(-max(uv.y - vShop.x, 0.0) / 4.0) * step(vShop.x, uv.y));
  }
  if (vDeco.x > 0.0) {
    // Light bands every so many floors, not on the base line or the roof line.
    float s = vDeco.x;
    float d = (fract(uv.y / s + 0.5) - 0.5) * s;
    float on = step(0.5 * s, uv.y) * step(0.5 * s, et) * clear;
    float bw = vC2.w < 0.5 ? 0.18 : 0.42;
    col += vTrimB.rgb * (sc_line(d, bw, fw.y) * sc_detail(s, fw.y * 4.0) * on * 0.75 * gain);
    col += vTrimB.rgb * (0.04 * exp(-abs(d) * 0.8) * on);
  }
  if (vDeco.y > 0.0) {
    // Fins: thin vertical tubes on the mullion lines.
    float s = vDeco.y;
    float d = (fract(uv.x / s + 0.5) - 0.5) * s;
    float on = step(0.5 * s, uv.x) * step(0.5 * s, W - uv.x) * clear;
    vec3 fin = fract(vC2.w * 3.0) < 0.5 ? vTrimA.rgb : vTrimB.rgb;
    col += fin * (sc_line(d, 0.1, fw.x) * sc_detail(s, fw.x * 4.0) * on * 0.7 * gain);
  }
  if (isMega) {
    // The spine: a wide band of light up the middle of every face, with dashes climbing it.
    float sx = abs(uv.x - 0.5 * W);
    float sw = max(2.5, 0.04 * W);
    float spine = (1.0 - smoothstep(sw - fw.x, sw + fw.x, sx)) * clear;
    float climb = 0.4 + 0.6 * sc_pulse(yAbs / 26.0 - uTime * 0.3, 0.5, fw.y / 26.0);
    col = mix(col, vTrimB.rgb * (1.35 * climb * uGlow), spine);
    col += vTrimA.rgb * (sb_tube(sx - sw - 1.6, 0.35, fw.x) * clear * gain);
    // Every tier ends in a broad band of light under its parapet.
    col += mix(vTrimA.rgb, vec3(1.3), 0.3) * (sc_line(et - 5.0, 1.5, fw.y) * 0.85 * gain);
  }
  return col;
}

void main() {
  vec3 p = vPu.xyz;
  vec2 raw = vec2(vPu.w, vNv.w);
  vec2 fw = max(fwidth(raw), vec2(1e-4));
  // Under MSAA a pixel on a silhouette is shaded at its centre, outside the triangle, so the
  // interpolated face coordinate overshoots, by hundreds of metres on a face seen edge-on.
  vec2 uv = clamp(raw, vec2(0.0), vFace.xy);
  vec3 n = normalize(vNv.xyz);
  vec3 v = p - vec3(0.0, cameraPosition.y, 0.0);
  vec3 vn = v / max(length(v), 1e-3);
  float style = vSeed.z;
  vec3 col;
  if (vFace.z > SB_FACE_TOP) col = sb_roof(uv, fw);
  else if (sb_is(style, SB_CROWN)) col = sb_crown(uv, fw);
  else if (sb_is(style, SB_SPIRE)) col = sb_spire(uv, fw);
  else col = sb_facade(uv, fw, vn, n);
  // A last guard: one runaway pixel would be smeared over the whole frame by the bloom.
  col = clamp(col, vec3(0.0), vec3(12.0));
  gl_FragColor = vec4(sc_fog(col, p), 1.0);
}
