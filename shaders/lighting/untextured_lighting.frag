#version 450

layout(location = 0) in vec2 fragCoord;
layout(location = 1) in vec3 fragNormal;

layout(binding = 1) uniform vec3 color;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(color, 0.0);
}