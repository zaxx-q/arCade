// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/streetlife/), adapted to GLSL 3.30 only by the prelude files.


// The blue half of an emergency strobe; the palette has no pure blue.
const vec3 SL_BLUE = vec3(0.03, 0.16, 1.0);

uniform highp sampler2D uLaneTex;
uniform vec2 uHandover;

// x mod y for whole numbers held in floats, safe where division is not correctly rounded.
float sl_imod(float x, float y) { return x - y * floor((x + 0.5) / y); }
// One lane: x signed offset from the road's centre line, y dir * laps, z car slots (0: no lane), w phase.
vec4 sl_lane(float line, float row) { return texelFetch(uLaneTex, ivec2(int(line + 0.5), int(row + 0.5)), 0); }
// A tile coordinate along the lane, in car slots measured from slot 0.
float sl_q(vec4 lane, float coord) { return (coord / SC_TILE - fract(lane.y * uLoop + lane.w)) * lane.z; }

struct SlCar {
  float offset;  // metres from the queried coordinate to the car's centre, along the lane's axis
  float show;    // 1 if the slot holds a vehicle
  float klass;   // 0 car, 1 van, 2 bus
  float kind;    // 0 plain, 1 taxi, 2 emergency
  float idx;     // car index in its lane
  vec3 r1;       // x jitter, y presence, z tints
  vec3 r2;       // x class, y kind, z accent
};

// Car number idxF (any whole number; it wraps) of a lane. q is sl_q of the coordinate being asked about.
SlCar sl_car(vec4 lane, float laneId, float row, float q, float idxF) {
  SlCar c;
  float n = max(lane.z, 1.0);
  float spacing = SC_TILE / n;
  c.idx = sl_imod(idxF, n);
  c.r1 = sc_hash33(vec3(laneId, c.idx, 1.0));
  c.r2 = sc_hash33(vec3(laneId, c.idx, 2.0));
  float jitter = clamp(1.0 - 16.0 / spacing, 0.0, 0.5);
  c.offset = (idxF - q + (c.r1.x - 0.5) * jitter) * spacing;
  c.show = step(0.5, lane.z) * step(0.12, c.r1.y);
  // Buses keep out of the fast inner lane.
  float inner = 1.0 - step(0.5, sl_imod(row, 3.0));
  c.klass = c.r2.x < 0.06 ? 2.0 - inner : (c.r2.x < 0.21 ? 1.0 : 0.0);
  c.kind = c.r2.y < 0.006 ? 2.0 : (c.r2.y < 0.066 ? 1.0 : 0.0);
  if (c.kind > 0.5) c.klass = 0.0;
  return c;
}

// Half length, height, half width, metres.
vec3 sl_dims(SlCar c) {
  if (c.klass > 1.5) return vec3(2.7, 2.1, 1.02) * vec3(2.13, 1.48, 1.24);
  if (c.klass > 0.5) return vec3(2.7, 2.1, 1.02);
  return vec3(2.25 * (0.94 + 0.12 * fract(c.r1.z * 17.0)), 1.26, 0.95);
}
// Headlights: ice white to gold.
vec3 sl_headColor(SlCar c) { return mix(sc_neon(7.0), sc_neon(4.0), 0.1 + 0.45 * fract(c.r1.z * 5.0)); }
// Tail lights: signal red, a few leaning magenta.
vec3 sl_tailColor(SlCar c) { return mix(sc_neon(6.0), sc_neon(0.0), 0.6 * step(0.78, fract(c.r1.z * 9.0))); }
vec3 sl_accent(SlCar c) {
  float a = c.r2.z;
  return sc_neon(a < 0.30 ? 0.0 : a < 0.58 ? 1.0 : a < 0.76 ? 2.0 : a < 0.88 ? 3.0 : a < 0.95 ? 5.0 : 4.0);
}
// Underglow strength: every car carries a little, a few are over-bright.
float sl_underglow(SlCar c) {
  float g = fract(c.r2.z * 13.0);
  float over = step(0.93, fract(c.r2.x * 29.0));
  return (0.3 + 0.9 * g * g) * (1.0 + 2.4 * over) * (c.klass > 0.5 ? 0.55 : 1.0);
}
// How much of a lamp shows from where the camera is. facing: 1 looking straight into the lamp, -1 from
// behind it. lift: sine of the camera's elevation above the road, seen from the car. From above, the
// glare of both ends of a car shows; from the street, only the end that points at you.
float sl_headShow(float facing, float lift) {
  return smoothstep(-0.05, 0.3, facing) * (0.3 + 0.7 * facing * facing) + 0.2 * smoothstep(0.15, 0.6, lift);
}
float sl_tailShow(float facing, float lift) {
  return smoothstep(-0.05, 0.3, -facing) * (0.45 - 0.55 * min(facing, 0.0)) + 0.24 * smoothstep(0.15, 0.6, lift);
}
// Wet asphalt throws light forward, not up: the pools are faint from the street and plain from the air.
float sl_poolShow(float lift) { return mix(0.45, 1.0, smoothstep(0.08, 0.5, lift)); }
// Emergency strobe in double flashes: x the red side, y the blue side.
vec2 sl_strobe(SlCar c) {
  float ph = fract(uTime * 1.9 + c.r1.z * 7.0);
  float blink = step(0.5, fract(ph * 6.0));
  return mix(vec2(0.3), vec2(1.0 - step(0.5, ph), step(0.5, ph)) * blink, uMotion);
}

attribute vec4 aSlot;   // axis, road offset from the nearest road, lane row within the axis, car offset from the nearest car

struct SlPlaced {
  SlCar car;
  vec3 anchor;     // tile position of the car's centre, on the road
  vec2 fwd;        // unit direction of travel in xz
  vec3 dims;
  float dist;      // from the camera, metres
  float fadeNear;  // 1 where real cars are drawn, 0 where the far dashes have taken over
  float along;     // tile coordinate along the lane
  float laneId;
};

SlPlaced sl_placed() {
  SlPlaced p;
  float axis = aSlot.x;
  float camAlong = axis < 0.5 ? uCam.x : uCam.z;
  float camAcross = axis < 0.5 ? uCam.z : uCam.x;
  float lineF = floor(camAcross / SC_CELL + 0.5) + aSlot.y;
  float line = sl_imod(lineF, SC_CELLS);
  float row = axis * 6.0 + aSlot.z;
  vec4 lane = sl_lane(line, row);
  p.laneId = line + row * 64.0;
  float q = sl_q(lane, camAlong);
  p.car = sl_car(lane, p.laneId, row, q, floor(q + 0.5) + aSlot.w);
  p.along = camAlong + p.car.offset;
  float across = lineF * SC_CELL + lane.x;
  p.anchor = axis < 0.5 ? vec3(p.along, 0.0, across) : vec3(across, 0.0, p.along);
  float dir = lane.y < 0.0 ? -1.0 : 1.0;
  p.fwd = axis < 0.5 ? vec2(dir, 0.0) : vec2(0.0, dir);
  p.dims = sl_dims(p.car);
  p.dist = length(vec3(sc_wrap(p.anchor.xz), uCam.y));
  p.fadeNear = 1.0 - smoothstep(uHandover.x, uHandover.y, p.dist);
  return p;
}
// A car-frame offset turned to the direction of travel.
vec3 sl_turn(vec2 fwd, vec3 v) { return vec3(fwd.x * v.x - fwd.y * v.z, v.y, fwd.y * v.x + fwd.x * v.z); }

attribute vec3 aAlt;
attribute float aPart;
varying vec3 vP;
varying vec3 vLocal;
flat varying vec4 vInfo;    // class, kind, part, tile coordinate along the lane
flat varying vec3 vDims;
flat varying vec4 vRand;    // paint, lamp style, trim, interior
flat varying vec3 vAccent;
flat varying vec3 vTint;    // the block's neon, mirrored in the paint
flat varying vec4 vTail;    // rgb tail colour, a underglow strength
flat varying vec3 vHead;
flat varying vec2 vStrobe;
void main() {
  SlPlaced c = sl_placed();
  vec3 shape = c.car.klass < 0.5
    ? position * vec3(c.dims.x / 2.25, 1.0, 1.0)
    : aAlt * (c.car.klass > 1.5 ? vec3(2.13, 1.48, 1.24) : vec3(1.0));
  vec3 p = sc_place(c.anchor, sl_turn(c.fwd, shape));
  vP = p;
  vLocal = shape;
  vInfo = vec4(c.car.klass, c.car.kind, aPart, c.along);
  vDims = c.dims;
  vec3 r3 = sc_hash33(vec3(c.laneId, c.car.idx, 3.0));
  vRand = vec4(r3, fract(c.car.r1.z * 3.0));
  vAccent = sl_accent(c.car);
  vTint = sc_neon(sc_cellData(sc_cell(c.anchor.xz)).g);
  vTail = vec4(sl_tailColor(c.car), sl_underglow(c.car));
  vHead = sl_headColor(c.car);
  vStrobe = sl_strobe(c.car);
  // Bodies are dark; past the handover the far dashes carry the lights and the body is not drawn.
  gl_Position = c.car.show > 0.5 && c.dist < uHandover.y ? sc_project(p) : vec4(2.0, 2.0, 2.0, 1.0);
}
