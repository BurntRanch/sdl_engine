#version 450

layout(location = 0) in vec2 vt_pos;
layout(location = 1) in vec2 vt_txcoord;

layout(location = 0) out vec2 fragCoord;
layout(location = 1) out vec2 fragPos;

void main() {
    gl_Position = vec4(vt_pos, 0.0, 1.0);

    fragCoord = vt_txcoord;
    fragPos = vec2(vt_pos.x, vt_pos.y); // This isn't what I'm supposed to do usually, but we're always guaranteed a fullscreen quad anyways.
}