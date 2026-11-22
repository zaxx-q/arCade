#version 330 core
// Written after pmndrs/postprocessing (zlib) and three.js (MIT) algorithms; notices in THIRD_PARTY_NOTICES.md.
// Bloom, step 1: keep what is brighter than the threshold (pmndrs LuminanceMaterial, colour output).
uniform sampler2D inputBuffer;
uniform float threshold;
uniform float smoothing;
in vec2 vUv;
out vec4 fragColor;
void main() {
  vec4 texel = texture(inputBuffer, vUv);
  float l = dot(vec3(0.2126729, 0.7151522, 0.0721750), texel.rgb);
  float mask = smoothstep(threshold, threshold + smoothing, l);
  fragColor = texel * mask;
}
