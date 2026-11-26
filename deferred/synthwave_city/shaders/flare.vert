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

// position.xy: the corner of a camera-facing sprite, -1..1.
varying vec2 vCorner;
varying vec3 vP;
flat varying vec4 vLook;     // palette index, seed, intensity, kind
flat varying vec2 vGain;     // x: brightness, y: glint (the beam is pointing at the viewer)

void main() {
  float kind = aShape.w;
  vec3 dir = bm_dir();
  vec2 rel = sc_wrap(aOrigin.xz);
  float alive = sc_farFade(rel);
  vec3 centre = sc_place(aOrigin, vec3(0.0));
  vec3 toCam = vec3(0.0, cameraPosition.y, 0.0) - centre;
  float dist = max(length(toCam), 1e-3);
  toCam /= dist;

  // How squarely the beam points at the viewer: a searchlight glints as it sweeps past.
  float glint = kind == BM_PILLAR ? 0.0 : pow(max(dot(dir, toCam), 0.0), kind == BM_LASER ? 220.0 : 28.0);

  float size = kind == BM_PILLAR ? aShape.y * 13.0 : kind == BM_LASER ? 3.5 : 7.0 + 14.0 * glint;
  float sizePix = sc_minSize(centre, size, kind == BM_PILLAR ? 6.0 : 3.0);
  // Lift the sprite towards the viewer so the roof it stands on does not slice it in half.
  centre += toCam * min(min(sizePix, 8.0), dist * 0.5);
  vec3 p = sc_sprite(centre, position.xy, sizePix * alive);

  float thin = size / sizePix;
  vCorner = position.xy;
  vP = p;
  vLook = vec4(aLook.x, aLook.y, aLook.z, kind);
  vGain = vec2(thin * thin * alive * aRoll.w, glint);
  gl_Position = sc_project(p);
}
