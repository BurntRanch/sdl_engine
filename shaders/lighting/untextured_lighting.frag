#version 450
#define MAX_LIGHTS 2048

#define PI 3.14159265359

//#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec2 fragCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 worldPos;

layout(std140, binding = 1) uniform MaterialUBO {
    vec3 color;
    float metallic;
    float roughness;
} material_ubo;

struct PointLight {
    vec3 position;
    vec3 color;
};

layout(std140, binding = 2) uniform LightsUBO {
    int pointLightCount;
    PointLight pointlights[MAX_LIGHTS];
} lights_ubo;

layout(std140, binding = 3) uniform CameraData {
    vec3 position;
} cameraData_ubo;

layout(location = 0) out vec4 outColor;

/* Huge thanks to https://learnopengl.com/PBR/Lighting !! */

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(cameraData_ubo.position - worldPos);

    vec3 color = normalize(material_ubo.color);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, color, material_ubo.metallic);

    vec3 finalLightValue = vec3(0);

    for (int i = 0; i < lights_ubo.pointLightCount; i++) {
        PointLight light = lights_ubo.pointlights[i];
        vec3 lightColor = light.color;
    
        vec3 L = normalize(light.position - worldPos);
        vec3 H = normalize(V + L);

        float distance    = length(light.position - worldPos);
        float attenuation = 1.0 / (distance * distance * distance * distance);
        vec3 radiance     = lightColor * attenuation;

        //debugPrintfEXT("radiance: %f %f %f\n", radiance.x, radiance.y, radiance.z);

        float NDF = DistributionGGX(N, H, material_ubo.roughness);
        float G = GeometrySmith(N, V, L, material_ubo.roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;

        kD *= 1.0 - material_ubo.metallic;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);
        finalLightValue += (kD * color / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.2) * color/* * ao*/;
    outColor = vec4(ambient + finalLightValue, 1);
}