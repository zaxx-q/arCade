// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 only by the prelude files.


uniform vec4 uCrowd;      // x where people start to shrink away, y where they are gone, z people per block face, w people per cell
varying vec3 vP;
varying vec2 vQ;          // metres: x across the figure, y up from the feet
flat varying vec4 vBody;  // stature, leg swing -1..1, bob, accent variant 0..1
flat varying vec4 vLook;  // umbrella, coat, cloth shade, lean
flat varying vec3 vAccent;
flat varying vec3 vRim;

float sl_imod(float x, float y) { return x - y * floor((x + 0.5) / y); }

// Dress a figure from its hashes and stand it at anchor, upright and turned about the vertical to face
// the camera. stride is the distance walked in radians of leg swing; palette the neon of its block.
void sl_figure(vec3 anchor, float scale, float stride, float moving, vec3 r2, vec3 r3, float palette) {
  float stature = mix(1.52, 1.9, r2.y);
  vBody = vec4(stature, sin(stride) * moving, abs(sin(stride)) * 0.035 * moving, r2.z);
  vLook = vec4(step(0.86, r3.x), step(0.62, r3.y), r3.z * r3.z, 0.0);
  float a = r3.x * 7.0 - floor(r3.x * 7.0);
  vAccent = sc_neon(a < 0.3 ? 0.0 : a < 0.6 ? 1.0 : a < 0.75 ? 4.0 : a < 0.87 ? 5.0 : a < 0.95 ? 3.0 : 7.0);
  vRim = sc_neon(palette);

  vec3 right = vec3(viewMatrix[0][0], 0.0, viewMatrix[2][0]);
  float rl = length(right);
  right = rl > 1e-4 ? right / rl : vec3(1.0, 0.0, 0.0);
  vec2 quad = vec2(position.x * 0.58, position.y * 2.6);
  vec3 p = sc_place(anchor, (right * quad.x + vec3(0.0, quad.y, 0.0)) * scale);
  vP = p;
  vQ = quad;
  gl_Position = scale > 0.0 ? sc_project(p) : vec4(2.0, 2.0, 2.0, 1.0);
}

attribute vec4 aBlock;    // cell offset from the camera's cell along x and z, slot in the cell
uniform highp sampler2D uAlleyTex;

void main() {
  float cellXF = floor(uCam.x / SC_CELL) + aBlock.x;
  float cellZF = floor(uCam.z / SC_CELL) + aBlock.y;
  float cx = sl_imod(cellXF, SC_CELLS), cz = sl_imod(cellZF, SC_CELLS);
  vec4 cell = sc_cellData(ivec2(int(cx + 0.5), int(cz + 0.5)));
  float district = floor(cell.r + 0.5);
  float share = (district < 0.5 ? 1.0 : district < 1.5 ? 0.7 : district < 2.5 ? 0.55
    : district < 3.5 ? 1.0 : 0.4);
  // The block: the cell minus the road corridors on its four sides.
  float x0 = cellXF * SC_CELL + sc_corridorHalf(sc_isAvenue(cx));
  float x1 = (cellXF + 1.0) * SC_CELL - sc_corridorHalf(sc_isAvenue(sl_imod(cx + 1.0, SC_CELLS)));
  float z0 = cellZF * SC_CELL + sc_corridorHalf(sc_isAvenue(cz));
  float z1 = (cellZF + 1.0) * SC_CELL - sc_corridorHalf(sc_isAvenue(sl_imod(cz + 1.0, SC_CELLS)));

  float id = cz * SC_CELLS + cx + 2048.0;
  float p = aBlock.z;
  vec3 r1 = sc_hash33(vec3(id, p, 11.0));
  vec3 r2 = sc_hash33(vec3(id, p, 12.0));
  vec3 r3 = sc_hash33(vec3(id, p, 13.0));
  vec3 r4 = sc_hash33(vec3(id, p, 14.0));

  vec2 at;
  float show, walked, moving;
  if (district > 2.5 && district < 3.5) {
    // A plaza: round rings about the centre, across it on straight lines, or standing about.
    show = step(p + 0.5, uCrowd.w * share);
    vec2 mid = vec2(x0 + x1, z0 + z1) * 0.5;
    vec2 halfSize = vec2(x1 - x0, z1 - z0) * 0.5;
    if (r4.x < 0.7000000000000001) {
      moving = 1.0 - step(0.55, r4.x);
      float radius = mix(24.0, 50.0, r4.y);
      float speed = 1.0 + 0.5 * r1.y;
      float laps = moving > 0.5 ? max(1.0, floor(speed * 4096.0 / (2.0 * SC_PI * radius))) : 0.0;
      float dir = r4.z < 0.5 ? -1.0 : 1.0;
      float u = fract(dir * laps * uLoop + r1.z);
      float a = 2.0 * SC_PI * u;
      at = mid + radius * vec2(cos(a), sin(a));
      walked = u * 2.0 * SC_PI * radius;
    } else {
      bool along0 = r4.z < 0.5;
      float ha = along0 ? halfSize.x : halfSize.y;
      float hp = along0 ? halfSize.y : halfSize.x;
      float sgn = fract(r4.z * 7.0) < 0.5 ? -1.0 : 1.0;
      float offset = sgn * (22.0 + (hp - 46.0) * r4.y);
      float laps = floor(12.0 + 11.99 * r1.y);
      float tri = abs(2.0 * fract(laps * uLoop + r1.z) - 1.0);
      float along = -ha + 1.0 + (2.0 * ha - 2.0) * tri;
      at = mid + (along0 ? vec2(along, offset) : vec2(offset, along));
      walked = tri * (2.0 * ha - 2.0);
      moving = 1.0;
    }
  } else {
    // An alley: slot p belongs to alley place p mod 6 (three per axis); the cell may have fewer.
    float a = sl_imod(p, 6.0);
    float axis = step(2.5, a);
    float k = a - 3.0 * axis;
    float q = floor((p - a) / 6.0 + 0.5);
    ivec2 tx = ivec2(int(cx + 0.5) * 4 + int(axis + 0.5) * 2, int(cz + 0.5));
    vec4 centres = texelFetch(uAlleyTex, tx, 0);
    vec4 halves = texelFetch(uAlleyTex, tx + ivec2(1, 0), 0);
    show = step(k + 0.5, centres.w) * step(q + 0.5, uCrowd.w / 6.0 * share);
    float centre = k < 0.5 ? centres.x : k < 1.5 ? centres.y : centres.z;
    float halfW = k < 0.5 ? halves.x : k < 1.5 ? halves.y : halves.z;
    // Some stand at a shop window; the rest walk the alley end to end and back.
    float standing = 1.0 - step(0.25, r2.x);
    moving = 1.0 - standing;
    float laps = standing > 0.5 ? 0.0 : floor(12.0 + 11.99 * r1.y);
    float tri = abs(2.0 * fract(laps * uLoop + r1.z) - 1.0);
    float lo = (axis < 0.5 ? x0 : z0) + 0.5, hi = (axis < 0.5 ? x1 : z1) - 0.5;
    float along = mix(lo, hi, tri);
    float lateral = standing > 0.5
      ? (r1.x < 0.5 ? -1.0 : 1.0) * max(halfW - 0.7, 0.0)
      : (2.0 * r1.x - 1.0) * max(halfW - 0.9, 0.0);
    float across = (axis < 0.5 ? cellZF : cellXF) * SC_CELL + centre + lateral;
    at = axis < 0.5 ? vec2(along, across) : vec2(across, along);
    walked = tri * (hi - lo);
  }

  vec3 anchor = vec3(at.x, 0.0, at.y);
  float dist = length(vec3(sc_wrap(anchor.xz), uCam.y));
  float scale = (1.0 - smoothstep(uCrowd.x, uCrowd.y, dist)) * show;
  sl_figure(anchor, scale, walked * (SC_PI / 0.72), moving, r2, r3, cell.g);
}
