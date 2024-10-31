#version 450

layout(location = 0) in vec2 fragCoord;

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main() {
    // https://github.com/GameMakerDiscord/blur-shaders

    float sampled = (texture(tex, fragCoord).r
        + texture(tex, fragCoord*0.99+0.5*0.01).r
        + texture(tex, fragCoord*0.98+0.5*0.02).r
        + texture(tex, fragCoord*0.97+0.5*0.03).r
        + texture(tex, fragCoord*0.96+0.5*0.04).r
        + texture(tex, fragCoord*0.95+0.5*0.05).r
        + texture(tex, fragCoord*0.94+0.5*0.06).r
        + texture(tex, fragCoord*0.93+0.5*0.07).r
        + texture(tex, fragCoord*0.92+0.5*0.08).r
        + texture(tex, fragCoord*0.91+0.5*0.09).r) * 0.1;

    outColor = vec4(1.0, 1.0, 1.0, sampled);
}