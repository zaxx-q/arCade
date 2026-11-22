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

uniform vec2 uLamp;   // x smallest half-size in pixels, y how much a lamp held at that size dims
varying vec2 vUv;
flat varying vec3 vColor;
flat varying vec2 vShape;   // x sharpness of the core, y streak amount
void main() {
  SlPlaced c = sl_placed();
  float id = position.z;
  vUv = position.xy;
  vColor = vec3(0.0);
  vShape = vec2(14.0, 0.0);
  gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
  if (c.car.show < 0.5 || c.fadeNear <= 0.0) return;

  float boxy = step(0.5, c.car.klass);
  float sideSign = mod(id, 2.0) < 0.5 ? -1.0 : 1.0;
  vec3 lamp;
  vec3 color;
  float size;
  float streak = 0.0;
  if (id < 1.5) {
    lamp = vec3(c.dims.x + 0.05, boxy > 0.5 ? 0.25 * c.dims.y : 0.39, sideSign * (boxy > 0.5 ? 0.68 : 0.63) * c.dims.z);
    color = sl_headColor(c.car) * 6.0;
    size = 0.6;
    streak = 1.0;
  } else if (id < 3.5) {
    lamp = vec3(-c.dims.x - 0.05, boxy > 0.5 ? 0.36 * c.dims.y : 0.66, sideSign * (boxy > 0.5 ? 0.8 : 0.62) * c.dims.z);
    color = sl_tailColor(c.car) * 4.0;
    size = 0.5;
  } else {
    if (c.car.kind < 0.5) return;
    lamp = vec3(-0.45, c.dims.y + 0.12, 0.0);
    if (c.car.kind > 1.5) {
      vec2 s = sl_strobe(c.car);
      color = (sc_neon(6.0) * s.x + SL_BLUE * s.y) * 10.0;
      size = 1.0;
    } else {
      color = sc_neon(4.0) * 2.5;
      size = 0.32;
    }
  }

  vec3 centre = sc_place(c.anchor, sl_turn(c.fwd, lamp));
  vec3 toCam = vec3(0.0, cameraPosition.y, 0.0) - centre;
  float range = length(toCam);
  toCam /= max(range, 1e-3);
  // Headlights throw forward and tail lights back; neither shows through its own car.
  float facing = dot(toCam, vec3(c.fwd.x, 0.0, c.fwd.y));
  float amount = 1.0;
  if (id < 1.5) amount = sl_headShow(facing, toCam.y);
  else if (id < 3.5) amount = sl_tailShow(facing, toCam.y);
  if (amount <= 0.0) return;

  // Stand the sprite clear of the bodywork, and never let it fall under a few pixels.
  centre += toCam * min(0.45, range * 0.5);
  float drawn = sc_minSize(centre, size, uLamp.x);
  float grow = size / drawn;
  vShape = vec2(mix(3.5, 14.0, smoothstep(0.5, 1.0, grow)), streak * grow);
  vColor = color * amount * c.fadeNear * mix(uLamp.y, 1.0, grow) * (1.0 - sc_fogAmount(centre)) * uGlow;
  gl_Position = sc_project(sc_sprite(centre, position.xy, drawn));
}
