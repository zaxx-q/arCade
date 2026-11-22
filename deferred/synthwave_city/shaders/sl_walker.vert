// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 only by the prelude files.


attribute vec4 aWalker;   // axis * 2 + side, road offset, block offset, index on that block face
uniform vec4 uCrowd;      // x where people start to shrink away, y where they are gone, z people per block face
varying vec3 vP;
varying vec2 vQ;          // metres: x across the figure, y up from the feet
flat varying vec4 vBody;  // stature, leg swing -1..1, bob, accent variant 0..1
flat varying vec4 vLook;  // umbrella, coat, cloth shade, lean
flat varying vec3 vAccent;
flat varying vec3 vRim;

float sl_imod(float x, float y) { return x - y * floor((x + 0.5) / y); }

void main() {
  float axis = step(1.5, aWalker.x);
  float side = aWalker.x - 2.0 * axis;
  float camAlong = axis < 0.5 ? uCam.x : uCam.z;
  float camAcross = axis < 0.5 ? uCam.z : uCam.x;
  float lineF = floor(camAcross / SC_CELL + 0.5) + aWalker.y;
  float blockF = floor(camAlong / SC_CELL + 0.5) + aWalker.z;
  float line = sl_imod(lineF, SC_CELLS);
  float block = sl_imod(blockF, SC_CELLS);
  float next = sl_imod(blockF + 1.0, SC_CELLS);
  float avenue = sc_isAvenue(line);

  // This face of the block: from one cross street's curb to the next.
  float lo = blockF * SC_CELL + sc_roadHalf(sc_isAvenue(block)) + 1.0;
  float hi = (blockF + 1.0) * SC_CELL - sc_roadHalf(sc_isAvenue(next)) - 1.0;

  // The block it fronts decides how crowded it is and what colour its light is.
  float cellAcross = sl_imod(lineF + side - 1.0, SC_CELLS);
  vec4 cell = sc_cellData(axis < 0.5 ? ivec2(int(block + 0.5), int(cellAcross + 0.5)) : ivec2(int(cellAcross + 0.5), int(block + 0.5)));
  float district = floor(cell.r + 0.5);
  float crowd = district < 0.5 ? 1.0 : district < 1.5 ? 0.7 : district < 2.5 ? 0.55
    : district < 3.5 ? 1.0 : 0.4;

  float id = line + aWalker.x * 64.0;
  vec3 r1 = sc_hash33(vec3(id, block, aWalker.w));
  vec3 r2 = sc_hash33(vec3(id, block, aWalker.w + 256.0));
  vec3 r3 = sc_hash33(vec3(id, block, aWalker.w + 512.0));

  // Walk the block face there and back for ever; some just stand.
  float standing = 1.0 - step(0.16, r2.x);
  float laps = standing > 0.5 ? 0.0 : floor(12.0 + 11.99 * r1.y);
  float tri = abs(2.0 * fract(laps * uLoop + r1.z) - 1.0);
  float along = mix(lo, hi, tri);
  float inner = sc_roadHalf(avenue) + 0.8;
  float outer = sc_corridorHalf(avenue) - 0.8;
  float across = lineF * SC_CELL + (side * 2.0 - 1.0) * mix(inner, outer, r1.x);
  vec3 anchor = axis < 0.5 ? vec3(along, 0.0, across) : vec3(across, 0.0, along);

  float dist = length(vec3(sc_wrap(anchor.xz), uCam.y));
  float scale = (1.0 - smoothstep(uCrowd.x, uCrowd.y, dist)) * step(aWalker.w + 0.5, uCrowd.z * crowd);

  float stride = tri * (hi - lo) * (SC_PI / 0.72);
  float moving = 1.0 - standing;
  float stature = mix(1.52, 1.9, r2.y);
  vBody = vec4(stature, sin(stride) * moving, abs(sin(stride)) * 0.035 * moving, r2.z);
  vLook = vec4(step(0.86, r3.x), step(0.62, r3.y), r3.z * r3.z, 0.0);
  float a = r3.x * 7.0 - floor(r3.x * 7.0);
  vAccent = sc_neon(a < 0.3 ? 0.0 : a < 0.6 ? 1.0 : a < 0.75 ? 4.0 : a < 0.87 ? 5.0 : a < 0.95 ? 3.0 : 7.0);
  vRim = sc_neon(cell.g);

  // Upright, turned about the vertical to face the camera.
  vec3 right = vec3(viewMatrix[0][0], 0.0, viewMatrix[2][0]);
  float rl = length(right);
  right = rl > 1e-4 ? right / rl : vec3(1.0, 0.0, 0.0);
  vec2 quad = vec2(position.x * 0.58, position.y * 2.6);
  vec3 p = sc_place(anchor, (right * quad.x + vec3(0.0, quad.y, 0.0)) * scale);
  vP = p;
  vQ = quad;
  gl_Position = scale > 0.0 ? sc_project(p) : vec4(2.0, 2.0, 2.0, 1.0);
}
