#version 450

layout(location = 0) in vec2 fragCoord;

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(tex, fragCoord);
}