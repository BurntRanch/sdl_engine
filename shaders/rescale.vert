#version 450

layout(location = 0) in vec2 vt_pos;
layout(location = 1) in vec2 vt_txcoord;

layout(location = 0) out vec2 fragCoord;

void main() {
    gl_Position = vec4(vt_pos, 1.0, 1.0);
    fragCoord = vt_txcoord;
}