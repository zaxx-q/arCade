// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/beams/), adapted to GLSL 3.30 only by the prelude files.

#define BM_SEARCHLIGHT 0.0
#define BM_PILLAR 1.0
#define BM_LASER 2.0
#define BM_PILLAR_HALO 8.0
#define BM_LASER_GLOW 4.0

attribute vec3 aOrigin;
attribute vec4 aShape;
attribute vec4 aLook;
attribute vec4 aTilt;
attribute vec4 aAzim;
attribute vec4 aFan;
attribute vec4 aRoll;

float bm_wave(float cyclesPerLoop, float phase) {
  return sin(SC_TAU * fract(cyclesPerLoop * uLoop) + phase);
}
vec3 bm_dir() {
  float th = aTilt.x + aTilt.y * bm_wave(aTilt.z, aTilt.w);
  float ph = aAzim.x + SC_TAU * fract(aAzim.y * uLoop) + aAzim.z * bm_wave(aAzim.w, aRoll.z);
  float st = sin(th), ct = cos(th), sp = sin(ph), cp = cos(ph);
  vec3 c = vec3(st * cp, ct, st * sp);
  // Two unit vectors across c; neither degenerates when c is vertical.
  vec3 e1 = vec3(-sp, 0.0, cp);
  vec3 e2 = vec3(ct * cp, -st, ct * sp);
  float a = aLook.w * (aFan.x + aFan.y * bm_wave(aFan.z, aFan.w));
  float ro = aRoll.x + SC_TAU * fract(aRoll.y * uLoop);
  vec3 u = cos(ro) * e1 + sin(ro) * e2;
  return cos(a) * c + sin(a) * u;
}

// position.x: -1..1 across the ribbon. position.y: 0..1 along it.
varying vec3 vP;
varying vec4 vCoord;     // x across, y along, z thin-line compensation, w view gain
flat varying vec4 vShape;
flat varying vec4 vLook;

void main() {
  float kind = aShape.w;
  float t = position.y;
  vec3 dir = bm_dir();
  // The anchor is the emitter, so the whole beam wraps and sinks as one object.
  vec3 p0 = sc_place(aOrigin, vec3(0.0));
  vec3 p1 = sc_place(aOrigin, dir * aShape.x);
  vec3 d = p1 - p0;
  float dl = length(d);
  vec3 axis = dl > 1e-3 ? d / dl : vec3(0.0, 1.0, 0.0);
  vec3 pt = mix(p0, p1, t);

  float r = mix(aShape.y, aShape.z, t);
  float w = r * (kind == BM_PILLAR ? BM_PILLAR_HALO : kind == BM_LASER ? BM_LASER_GLOW : 1.0);
  // Never thinner than a pixel or so: widen the ribbon and dim it by the same factor instead.
  float wPix = sc_minSize(pt, w, kind == BM_LASER ? 1.3 : 1.25);
  vec3 p = sc_axisBillboard(pt, axis, position.x, wPix);

  // Seen nearly end-on the ribbon degenerates, so it fades there and the flare takes over. A
  // little before that, the longer path through the beam makes it brighter.
  vec3 toCam = normalize(vec3(0.0, cameraPosition.y, 0.0) - pt);
  float sinA = length(cross(axis, toCam));
  float gain = sc_axisFacing(pt, axis) * (1.0 + 0.5 * (1.0 - smoothstep(0.3, 0.95, sinA)));

  vP = p;
  vCoord = vec4(position.x, t, w / wPix, gain);
  vShape = aShape;
  vLook = aLook;
  gl_Position = sc_project(p);
}
