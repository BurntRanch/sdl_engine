#version 450

layout(binding = 0) uniform MatricesUniformBufferObject {
    mat4 viewMatrix;
    mat4 modelMatrix;
    mat4 projectionMatrix;
} matricesUBO;

layout(binding = 1) uniform WaypointsUniformBufferObject {
    vec3 Position;
} waypointUBO;

layout(location = 0) in vec2 fragCoord;
layout(location = 1) in vec2 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 clipSpacePosition = matricesUBO.projectionMatrix * matricesUBO.viewMatrix * vec4(waypointUBO.Position, 1.0);

    if (clipSpacePosition.w < 0) {
        discard;
    }

    clipSpacePosition /= clipSpacePosition.w;

    if ((clipSpacePosition.x - 0.1 > fragPos.x || fragPos.x > clipSpacePosition.x + 0.1)
     || (clipSpacePosition.y - 0.1 > fragPos.y || fragPos.y > clipSpacePosition.y + 0.1)) {
        discard;
    }
 
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
