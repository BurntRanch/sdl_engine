#version 450

layout(location = 0) in vec2 vt_pos;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(vt_pos, 0.0, 1.0);
    fragColor = vec3(vt_pos, 1.0);
}