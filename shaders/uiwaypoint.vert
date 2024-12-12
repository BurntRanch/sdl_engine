#version 450

layout(location = 0) in vec3 vt_pos;
layout(location = 1) in vec2 vt_txcoord;

layout(location = 0) out vec2 fragCoord;
layout(location = 1) out vec2 fragPos;

void main() {
    gl_Position = vec4(vt_pos, 1.0);

    fragCoord = vt_txcoord;
    fragPos = vt_pos.xy; // This isn't what I'm supposed to do usually, but we're always guaranteed a fullscreen quad anyways.
}