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

// Geometry: face id, u 0..1, v 0..1 (see ../geometry.ts).
attribute vec3 aFace;
// Instance: centre x, base y, centre z.
attribute vec3 aPos;
// Instance: full size.
attribute vec3 aSize;
// Instance: style, palette index A, palette index B, flags.
attribute vec4 aStyle;
// Instance: box seed, building seed, roof height of the whole building.
attribute vec3 aSeed;

void main() {
  float style = aStyle.x;
  float flags = aStyle.w;
  float fStreet = sb_bit(flags, SB_FLAG_STREET);
  float fRound = sb_bit(flags, SB_FLAG_ROUND);
  float faceId = aFace.x;
  bool top = faceId > SB_FACE_TOP;
  // Face size in metres. A cylinder wall is unrolled: its width is the circumference.
  float W = top ? aSize.x : (fRound > 0.5 ? SC_PI * aSize.x : (faceId < 1.5 ? aSize.z : aSize.x));
  float H = top ? aSize.z : aSize.y;

  vec3 p = sc_place(aPos, position * aSize);
  gl_Position = sc_project(p);
  vPu = vec4(p, aFace.y * W);
  vNv = vec4(normal, aFace.z * H);
  vFace = vec4(W, H, faceId, flags);

  // Seeds as exact integers: the hashes read float bits, so they must not be interpolated values.
  float xq = floor(aSeed.x * 16384.0);
  float bq = floor(aSeed.y * 16384.0);
  float fq = xq * 8.0 + faceId;
  vSeed = vec4(fq, bq, style, aPos.y);

  // b*: shared by every box of the building, so a setback tower reads as one design.
  vec3 hb = sc_hash33(vec3(bq, 11.0, 1.0));
  vec3 hc = sc_hash33(vec3(bq, 23.0, 2.0));
  vec3 hd = sc_hash33(vec3(bq, 31.0, 3.0));
  vec3 he = sc_hash33(vec3(bq, 43.0, 4.0));
  // x: per box. f, g, h: per face.
  vec3 hx = sc_hash33(vec3(xq, 37.0, 5.0));
  vec3 hf = sc_hash33(vec3(fq, 41.0, 6.0));
  vec3 hg = sc_hash33(vec3(fq, 53.0, 7.0));
  vec3 hh = sc_hash33(vec3(fq, 61.0, 8.0));

  bool isMega = sb_is(style, SB_MEGA);
  bool isStrip = sb_is(style, SB_STRIP);
  bool isKiosk = sb_is(style, SB_KIOSK);

  // ---- window grid: cell size, glazed share, lit share
  vec2 cell = vec2(3.0, 3.6);
  vec2 duty = vec2(0.6, 0.55);
  float litP = 0.45;
  if (sb_is(style, SB_SHAFT)) {
    cell = vec2(mix(2.4, 3.6, hb.x), mix(3.3, 4.0, hb.y));
    duty = vec2(mix(0.42, 0.74, hb.z), mix(0.42, 0.66, hc.x));
    litP = mix(0.3, 0.62, hc.y);
  } else if (sb_is(style, SB_GLASS)) {
    cell = vec2(mix(1.5, 2.8, hb.x), mix(3.5, 4.2, hb.y));
    duty = vec2(mix(0.86, 0.94, hb.z), mix(0.86, 0.94, hc.x));
    litP = mix(0.3, 0.6, hc.y);
  } else if (sb_is(style, SB_SLAB)) {
    cell = vec2(mix(4.5, 11.0, hb.x), mix(3.2, 3.9, hb.y));
    duty = vec2(1.0, mix(0.36, 0.6, hb.z));
    litP = mix(0.38, 0.72, hc.y);
  } else if (isStrip) {
    cell = vec2(mix(2.0, 3.6, hb.x), mix(7.0, 16.0, hb.y));
    duty = vec2(mix(0.3, 0.6, hb.z), 1.0);
    litP = mix(0.42, 0.78, hc.y);
  } else if (sb_is(style, SB_PODIUM)) {
    cell = vec2(mix(3.6, 6.5, hb.x), mix(4.0, 5.2, hb.y));
    duty = vec2(mix(0.7, 0.9, hb.z), mix(0.55, 0.8, hc.x));
    litP = mix(0.5, 0.85, hc.y);
  } else if (isMega) {
    cell = vec2(mix(2.8, 3.6, hb.x), mix(3.8, 4.4, hb.y));
    duty = vec2(mix(0.55, 0.8, hb.z), mix(0.5, 0.7, hc.x));
    litP = mix(0.34, 0.52, hc.y);
  } else if (isKiosk) {
    cell = vec2(2.6, H);
    duty = vec2(0.86, 0.8);
    litP = 1.0;
  }
  // A whole number of cells on the face, so no window is cut by a corner.
  vec2 dims = vec2(W, H);
  cell = dims / max(vec2(1.0), floor(dims / cell + 0.5));
  vGrid = vec4(cell, duty);
  // Zones: blocks of windows that share a mood (dark, busy, accent colour). They survive distance.
  vec2 zone = max(vec2(1.0), floor(vec2(mix(9.0, 30.0, hx.x), mix(14.0, 64.0, hx.y)) / cell + 0.5));
  vLit = vec4(litP, zone, mix(0.08, 0.35, hx.z));

  // ---- neon structure
  float floorH = cell.y / max(1.0, floor(cell.y / 3.8 + 0.5));
  float bandS = 0.0;
  if (he.x > 0.45 || isMega) bandS = max(1.0, floor(mix(4.0, 15.0, fract(he.x * 7.0)) * floorH / cell.y + 0.5)) * cell.y;
  float finS = 0.0;
  if (he.y < (fRound > 0.5 ? 0.7 : 0.36) && !isKiosk) finS = max(1.0, floor(mix(6.0, 22.0, fract(he.y * 9.0)) / cell.x + 0.5)) * cell.x;
  float tw = mix(0.14, 0.3, hd.x) * (1.0 + aSeed.z / 300.0);
  float gain = mix(1.3, 2.0, hd.y);
  if (isMega || sb_bit(flags, SB_FLAG_LANDMARK) > 0.5) { tw = 0.8; gain = 2.2; }
  if (isKiosk) tw = 0.1;
  vDeco = vec4(bandS, finS, tw, gain);

  // ---- colours
  vec3 cA = sc_neon(aStyle.y);
  vec3 cB = sc_neon(aStyle.z);
  vec3 wA = sb_even(cA, 0.5);
  vec3 wB = sb_even(cB, 0.5);
  vec3 warm = vec3(1.0, 0.5, 0.15);
  vec3 cool = vec3(0.3, 0.82, 1.0);
  vec3 c1, c2;
  if (hc.z < 0.3) { c1 = warm * 0.9; c2 = wA * 0.7; }
  else if (hc.z < 0.5) { c1 = cool * 0.8; c2 = wB * 0.7; }
  else if (hc.z < 0.78) { c1 = wA * 0.7; c2 = wB * 0.7; }
  else { c1 = mix(wA, warm, 0.5) * 0.8; c2 = cool * 0.8; }
  vC1 = vec4(c1, aSeed.z);
  vC2 = vec4(c2, hd.z);

  // ---- shop front along the street
  float hS = 0.0, bayW = 1.0;
  if (!top && fStreet > 0.5 && !sb_is(style, SB_SPIRE) && !sb_is(style, SB_CROWN)) {
    hS = cell.y * max(1.0, floor(6.6 / cell.y + 0.5));
    if (isStrip) hS = 6.5;
    if (isMega) hS = cell.y * 3.0;
    if (isKiosk) hS = H;
    hS = min(hS, H);
    bayW = W / max(1.0, floor(W / mix(5.0, 9.0, hf.x) + 0.5));
  }
  vShop = vec4(hS, bayW, fq, mix(1.3, 2.0, hg.x));

  // ---- sign panel
  vec4 panel = vec4(0.0);
  vec4 panelK = vec4(0.0);
  float idxA = floor(hh.y * 7.0);
  float idxB = mod(idxA + 1.0 + floor(hh.z * 5.0), 7.0);
  bool plain = !top && (style < 4.5 || isMega);
  float availH = H - hS;
  float effect = he.z;
  if (plain && effect < 0.075 && style > 0.5 && !isMega && !sb_is(style, SB_SLAB) && availH > 24.0 && W > 9.0) {
    // An effect tower: the whole facade plays one pattern, with its windows as the pixels.
    float kind = effect < 0.03 ? 1.0 : effect < 0.047 ? 5.0 : effect < 0.062 ? 0.0 : 3.0;
    panel = vec4(0.0, hS, W, availH);
    panelK = vec4(kind + SB_WHOLE, cell.x, floorH, bq);
    idxA = aStyle.y;
    idxB = aStyle.z;
  } else if (plain && fRound < 0.5 && W >= 14.0 && availH >= 9.0
      && hf.y < (isMega ? 0.9 : sb_is(style, SB_PODIUM) ? 0.5 : 0.3)) {
    float nCols = floor(W / cell.x + 0.5);
    float margin = max(1.0, floor(nCols * mix(0.08, 0.24, hg.x)));
    float nRows = floor(availH / cell.y + 0.01);
    float want = isMega ? mix(40.0, 130.0, hg.y) : mix(12.0, 60.0, hg.y * hg.y);
    float rows = clamp(floor(want / cell.y + 0.5), 1.0, max(1.0, nRows - 1.0));
    float freeRows = nRows - rows;
    if (nCols - 2.0 * margin >= 2.0 && freeRows >= 0.0) {
      float pw = (nCols - 2.0 * margin) * cell.x;
      float ph = rows * cell.y;
      float r0 = min(floor(hg.z * (freeRows + 1.0)), freeRows);
      float led = mix(0.6, 1.2, hh.x) * (isMega ? 2.0 : 1.0);
      vec2 pitch = vec2(pw, ph) / max(vec2(6.0), floor(vec2(pw, ph) / led + 0.5));
      panel = vec4(margin * cell.x, hS + r0 * cell.y, pw, ph);
      panelK = vec4(floor(hf.z * 8.0), pitch, fq);
    }
  } else if (plain && fRound > 0.5 && availH >= 30.0 && hf.y < 0.45) {
    // A round tower wears its sign as a ring of light.
    float rows = hg.y < 0.5 ? 1.0 : 2.0;
    float nRows = floor(availH / cell.y + 0.01);
    float r0 = floor(mix(0.3, 0.85, hg.z) * (nRows - rows));
    float ph = rows * cell.y;
    float led = ph / (rows > 1.5 ? 18.0 : 9.0);
    float kind = hf.z < 0.4 ? 7.0 : hf.z < 0.6 ? 0.0 : hf.z < 0.8 ? 3.0 : 4.0;
    panel = vec4(0.0, hS + r0 * cell.y, W, ph);
    panelK = vec4(kind, W / max(6.0, floor(W / led + 0.5)), led, fq);
  }
  vPanel = panel;
  vPanelK = panelK;

  vTrimA = vec4(sb_even(cA, 0.75), idxA);
  vTrimB = vec4(sb_even(cB, 0.75), idxB);
}
