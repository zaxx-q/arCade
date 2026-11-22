#version 330 core
// Written after pmndrs/postprocessing (zlib) and three.js (MIT) algorithms; notices in THIRD_PARTY_NOTICES.md.
// Bloom, step 3: 9-tap tent upsample, mixed with the matching downsampled level (pmndrs UpsamplingMaterial).
// texelSize is that of the INPUT (the smaller) image.
uniform sampler2D inputBuffer;
uniform sampler2D supportBuffer;
uniform vec2 texelSize;
uniform float radius;
in vec2 vUv;
out vec4 fragColor;
void main() {
  vec4 c = vec4(0.0);
  c += texture(inputBuffer, vUv + texelSize * vec2(-1.0, 1.0)) * 0.0625;
  c += texture(inputBuffer, vUv + texelSize * vec2(0.0, 1.0)) * 0.125;
  c += texture(inputBuffer, vUv + texelSize * vec2(1.0, 1.0)) * 0.0625;
  c += texture(inputBuffer, vUv + texelSize * vec2(-1.0, 0.0)) * 0.125;
  c += texture(inputBuffer, vUv) * 0.25;
  c += texture(inputBuffer, vUv + texelSize * vec2(1.0, 0.0)) * 0.125;
  c += texture(inputBuffer, vUv + texelSize * vec2(-1.0, -1.0)) * 0.0625;
  c += texture(inputBuffer, vUv + texelSize * vec2(0.0, -1.0)) * 0.125;
  c += texture(inputBuffer, vUv + texelSize * vec2(1.0, -1.0)) * 0.0625;
  vec4 baseColor = texture(supportBuffer, vUv);
  fragColor = mix(baseColor, c, radius);
}
