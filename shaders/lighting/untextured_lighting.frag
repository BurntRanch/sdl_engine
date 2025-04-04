#version 450
#define MAX_LIGHTS 2048

layout(location = 0) in vec2 fragCoord;
layout(location = 1) in vec3 fragNormal;

layout(binding = 1) uniform MaterialUBO {
    vec3 color;
} material_ubo;

struct PointLight {
    vec3 color;
    vec3 attenuation;
};

layout(std140, binding = 2) uniform LightsUBO {
    int pointLightCount;
    PointLight pointlights[MAX_LIGHTS];
} lights_ubo;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(normalize(lights_ubo.pointlights[0].color.xyz), lights_ubo.pointLightCount);
}