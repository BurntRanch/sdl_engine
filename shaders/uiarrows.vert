#version 450

layout(location = 0) in vec3 vt_pos;
layout(location = 2) in vec2 vt_txcoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 viewMatrix;
    mat4 modelMatrix;
    mat4 projectionMatrix;
} ubo;

layout(location = 0) out vec2 fragCoord;

void main() {
    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(vt_pos, 1.0);
    fragCoord = vt_txcoord;
}