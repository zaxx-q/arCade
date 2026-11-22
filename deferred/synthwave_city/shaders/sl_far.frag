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

uniform vec2 uLamp;   // as the lamp sprites: x smallest half-size in pixels, y how much a lamp held at that size dims
uniform vec4 uDash;   // x smallest sigma of a far lamp in pixels, y pool gain, z tail smear gain, w underglow gain
varying vec2 vRel;
varying float vAcross;
flat varying vec2 vRoad;

// A soft spot of light on the road, filtered against the pixel footprint fw. "at" is the offset from
// its centre (along, across), sigma its size. minPx keeps it from falling under that many pixels;
// power 1 conserves its energy when it is enlarged, less keeps it bright.
float sl_spot(vec2 at, vec2 sigma, vec2 fw, float minPx, float power) {
  vec2 s = max(sqrt(sigma * sigma + 0.25 * fw * fw), minPx * fw);
  vec2 u = at / s;
  return exp(-0.5 * dot(u, u)) * pow(sigma.x * sigma.y / (s.x * s.y), power);
}

void main() {
  float axis = vRoad.x, line = vRoad.y;
  float relAlong = axis < 0.5 ? vRel.x : vRel.y;
  float relAcross = axis < 0.5 ? vRel.y : vRel.x;
  vec2 fw = max(vec2(fwidth(relAlong), fwidth(vAcross)), 1e-4);

  // Which lane this pixel is in. x first lane offset, y lane pitch, z lanes per direction.
  vec3 geom = mix(vec3(2.0, 4.0, 2.0), vec3(4.0, 4.0, 3.0), sc_isAvenue(line));
  float slot = clamp(floor((abs(vAcross) - geom.x) / geom.y + 0.5), 0.0, geom.z - 1.0);
  float row = axis * 6.0 + step(0.0, vAcross) * 3.0 + slot;
  vec4 lane = sl_lane(line, row);
  float laneId = line + row * 64.0;
  float dl = vAcross - lane.x;
  float q = sl_q(lane, (axis < 0.5 ? uCam.x : uCam.z) + relAlong);
  float i0 = floor(q);
  float dir = lane.y < 0.0 ? -1.0 : 1.0;

  vec3 col = vec3(0.0);
  // The two cars either side of this pixel; nothing a car lights reaches further than that.
  for (int j = 0; j < 2; j++) {
    SlCar car = sl_car(lane, laneId, row, q, i0 + float(j));
    float halfL = sl_dims(car).x;
    float x = -car.offset * dir;                    // metres ahead of the car's centre
    float carAlong = relAlong + car.offset;         // the car's centre, relative to the camera
    float dist = length(vec3(carAlong, relAcross - dl, uCam.y));
    float amount = smoothstep(uHandover.x, uHandover.y, dist) * car.show;
    float facing = -dir * carAlong / max(dist, 1.0);   // 1 when it drives straight at the camera
    float lift = uCam.y / max(dist, 1.0);
    vec3 head = sl_headColor(car), tail = sl_tailColor(car);
    // Lamps: the two of each end drawn as one dot, held at a few pixels and dimmed as the sprites are.
    float lampGain = 1.6 * mix(uLamp.y, 1.0, clamp(0.6 / (uLamp.x * min(fw.x, fw.y)), 0.0, 1.0));
    vec3 c = head * sl_spot(vec2(x - halfL - 0.3, dl), vec2(0.5), fw, uDash.x, 0.0) * sl_headShow(facing, lift) * 6.0 * lampGain;
    c += tail * sl_spot(vec2(x + halfL + 0.3, dl), vec2(0.5), fw, uDash.x, 0.0) * sl_tailShow(facing, lift) * 4.0 * lampGain;
    // Light on the road, with the energy of the near quads.
    float pool = sl_poolShow(lift);
    c += mix(head, sc_neon(4.0), 0.25) * sl_spot(vec2(x - halfL - 6.5, dl), vec2(4.0, 1.2), fw, 0.0, 1.0) * uDash.y * pool;
    c += tail * sl_spot(vec2(x + halfL + 3.0, dl), vec2(2.2, 0.9), fw, 0.0, 1.0) * uDash.z * pool;
    vec3 under = sl_accent(car) * sl_underglow(car);
    if (car.kind > 1.5) {
      vec2 s = sl_strobe(car);
      under = (sc_neon(6.0) * s.x + SL_BLUE * s.y) * 6.0;
    }
    c += under * sl_spot(vec2(x, dl), vec2(halfL * 0.75, 1.25), fw, 0.0, 1.0) * uDash.w;
    col += c * amount;
  }

  // Only the two nearest cars are summed; where the footprint spans more of them, make up the rest.
  float spacing = SC_TILE / max(lane.z, 1.0);
  col *= clamp(1.25 * uDash.x * fw.x / spacing, 1.0, 2.5);

  vec3 p = vec3(vRel.x, 0.04, vRel.y);
  col *= sc_farFade(vRel) * (1.0 - sc_fogAmount(p)) * uGlow;
  gl_FragColor = vec4(col, 1.0);
}
