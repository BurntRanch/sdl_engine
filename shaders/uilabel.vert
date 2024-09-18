#version 450

layout(location = 0) in vec2 vt_pos;
layout(location = 1) in vec2 vt_txcoord;

layout(binding = 0) uniform UniformBufferObject {
    vec2 PositionOffset;
    float Depth;
} ubo;

layout(location = 0) out vec2 fragCoord;

void main() {
    ubo.PositionOffset *= 2;

    gl_Position = vec4(vt_pos.xy + ubo.PositionOffset, ubo.Depth, 1.0);
    fragCoord = vt_txcoord;
}