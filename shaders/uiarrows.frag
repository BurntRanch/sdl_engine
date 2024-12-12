#version 450

layout(location = 0) in vec2 fragCoord;

layout(binding = 1) uniform ArrowInfo {
    vec3 Color;
} arrowInfo;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(arrowInfo.Color, 1.0f);
}