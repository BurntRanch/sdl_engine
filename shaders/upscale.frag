#version 450

layout(location = 0) in vec2 fragCoord;

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(tex, fragCoord * vec2(800.0/1920.0, 600.0/1080.0));
}