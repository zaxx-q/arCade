#version 330 core
// Prelude for scene vertex shaders: gives the web version's WebGL-style code (GLSL ES 3.00 through three.js)
// the names it expects. Scene shaders are prelude_vs + defines + common (+ common_vertex) + body.
#define attribute in
#define varying out
layout(location = 0) in vec3 position;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform vec3 cameraPosition;
