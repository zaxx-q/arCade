#version 330 core
// Prelude for scene fragment shaders; see prelude_vs.glsl.
#define varying in
#define texture2D texture
layout(location = 0) out vec4 swc_fragColor;
#define gl_FragColor swc_fragColor
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform vec3 cameraPosition;
