#version 330 core
// Written after pmndrs/postprocessing (zlib) and three.js (MIT) algorithms; notices in THIRD_PARTY_NOTICES.md.
// Bloom, step 2: 13-tap downsample (pmndrs DownsamplingMaterial). texelSize is that of the INPUT image.
uniform sampler2D inputBuffer;
uniform vec2 texelSize;
in vec2 vUv;
out vec4 fragColor;
#define WEIGHT_INNER 0.125
#define WEIGHT_OUTER 0.05556
float clampToBorder(const in vec2 uv) { return float(uv.s >= 0.0 && uv.s <= 1.0 && uv.t >= 0.0 && uv.t <= 1.0); }
void main() {
  vec2 uv00 = vUv + texelSize * vec2(-1.0, 1.0);
  vec2 uv01 = vUv + texelSize * vec2(1.0, 1.0);
  vec2 uv02 = vUv + texelSize * vec2(-1.0, -1.0);
  vec2 uv03 = vUv + texelSize * vec2(1.0, -1.0);
  vec2 uv04 = vUv + texelSize * vec2(-2.0, 2.0);
  vec2 uv05 = vUv + texelSize * vec2(0.0, 2.0);
  vec2 uv06 = vUv + texelSize * vec2(2.0, 2.0);
  vec2 uv07 = vUv + texelSize * vec2(-2.0, 0.0);
  vec2 uv08 = vUv + texelSize * vec2(2.0, 0.0);
  vec2 uv09 = vUv + texelSize * vec2(-2.0, -2.0);
  vec2 uv10 = vUv + texelSize * vec2(0.0, -2.0);
  vec2 uv11 = vUv + texelSize * vec2(2.0, -2.0);
  vec4 c = vec4(0.0);
  vec4 w = WEIGHT_INNER * vec4(clampToBorder(uv00), clampToBorder(uv01), clampToBorder(uv02), clampToBorder(uv03));
  c += w.x * texture(inputBuffer, uv00);
  c += w.y * texture(inputBuffer, uv01);
  c += w.z * texture(inputBuffer, uv02);
  c += w.w * texture(inputBuffer, uv03);
  w = WEIGHT_OUTER * vec4(clampToBorder(uv04), clampToBorder(uv05), clampToBorder(uv06), clampToBorder(uv07));
  c += w.x * texture(inputBuffer, uv04);
  c += w.y * texture(inputBuffer, uv05);
  c += w.z * texture(inputBuffer, uv06);
  c += w.w * texture(inputBuffer, uv07);
  w = WEIGHT_OUTER * vec4(clampToBorder(uv08), clampToBorder(uv09), clampToBorder(uv10), clampToBorder(uv11));
  c += w.x * texture(inputBuffer, uv08);
  c += w.y * texture(inputBuffer, uv09);
  c += w.z * texture(inputBuffer, uv10);
  c += w.w * texture(inputBuffer, uv11);
  c += WEIGHT_OUTER * texture(inputBuffer, vUv);
  fragColor = c;
}
