#version 450

layout(location = 0) in vec2 vt_pos;
layout(location = 1) in vec2 vt_txcoord;

layout(binding = 0) uniform UniformBufferObject {
    vec4 Dimensions;
    float Depth;
} ubo;

layout(location = 0) out vec2 fragCoord;

void main() {
    if (gl_VertexIndex == 0 || gl_VertexIndex == 3) {
        gl_Position = vec4(ubo.Dimensions.x, ubo.Dimensions.y, ubo.Depth, 1.0);
    } else if (gl_VertexIndex == 1 || gl_VertexIndex == 5) {
        gl_Position = vec4(ubo.Dimensions.x + ubo.Dimensions.z, ubo.Dimensions.y + ubo.Dimensions.w, ubo.Depth, 1.0);
    } else if (gl_VertexIndex == 2) {
        gl_Position = vec4(ubo.Dimensions.x, ubo.Dimensions.y + ubo.Dimensions.w, ubo.Depth, 1.0);
    } else {
        gl_Position = vec4(ubo.Dimensions.x + ubo.Dimensions.z, ubo.Dimensions.y, ubo.Depth, 1.0);
    }

    fragCoord = vt_txcoord;
}