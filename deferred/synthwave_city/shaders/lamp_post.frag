// Synthwave City (c) Kris E. Granholm, MIT licence - https://github.com/highsierralabs/Synthwave_City
// Shader text taken from the web version (src/layers/ground/), adapted to GLSL 3.30 only by the prelude files.

varying vec3 vP;
varying float vPart;
varying float vUp;
flat varying float vCol;
void main() {
  vec3 lamp = sc_neon(vCol);
  // The pole is unlit metal that catches a little of its own lamp near the top.
  vec3 pole = vec3(0.016, 0.013, 0.034) + uAmbient * 0.4 + lamp * (0.1 * smoothstep(0.55, 1.0, vUp));
  vec3 head = mix(lamp, sc_neon(7.0), 0.5) * (7.0 * uGlow);
  gl_FragColor = vec4(sc_fog(mix(pole, head, step(0.5, vPart)), vP), 1.0);
}
