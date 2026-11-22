#version 330 core
// One big triangle covers the screen; vUv runs 0..1 over the visible part (the web version's full-screen quad).
layout(location = 0) in vec3 position;
out vec2 vUv;
void main() {
  vUv = position.xy * 0.5 + 0.5;
  gl_Position = vec4(position.xy, 0.0, 1.0);
}
