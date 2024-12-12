#version 450

layout(location = 0) in vec2 vt_pos;
layout(location = 1) in vec2 vt_txcoord;

layout(set = 0, binding = 0) uniform LabelUBO {
    vec2 PositionOffset;
    float Depth;
} labelUBO;

layout(set = 0, binding = 2) uniform GlyphUBO {
    vec2 Offset;
} glyphUBO;

layout(location = 0) out vec2 fragCoord;

void main() {
    gl_Position = vec4(vt_pos.xy + labelUBO.PositionOffset + glyphUBO.Offset, labelUBO.Depth, 1.0);
    fragCoord = vt_txcoord;
}