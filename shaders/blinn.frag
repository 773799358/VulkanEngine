#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_OES_standard_derivatives : enable
#extension GL_EXT_shader_texture_lod: enable

#define PI 3.1416

layout(set = 0, binding = 1) uniform UniformBufferObject
{
    vec3 viewPos;
	float ambientStrength;
	vec3 directionalLightPos;
	float padding;
	vec3 directionalLightColor;
} ubo;

layout(location = 0) in highp vec3 inColor;
layout(location = 1) in highp vec3 inNormal;
layout(location = 2) in highp vec2 inTexCoord;
layout(location = 3) in highp vec3 inWorldPos;
layout(location = 4) in highp vec3 inTangent;

layout(set = 1, binding = 0) uniform sampler2D baseColorTextureSampler;
layout(set = 1, binding = 1) uniform sampler2D normalTextureSampler;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTextureSampler;

// layout(location = 0)修饰符明确framebuffer的索引
layout(location = 0) out highp vec4 outColor;

vec3 T;
vec3 B;

highp vec3 Uncharted2Tonemap(highp vec3 x)
{
    highp float A = 0.15;
    highp float B = 0.50;
    highp float C = 0.10;
    highp float D = 0.20;
    highp float E = 0.02;
    highp float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

highp vec3 calculateNormal()
{
    if(length(inTangent) < 0.00001 || isnan(inTangent.x))
    {
        return inNormal;
    }

    highp vec3 tangentNormal = texture(normalTextureSampler, inTexCoord).xyz * 2.0 - 1.0;

    highp vec3 N = normalize(inNormal);
    highp vec3 T = normalize(inTangent.xyz);
    highp vec3 B = normalize(cross(N, T));

    highp mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

void main() 
{
    vec3 ambient = vec3(ubo.ambientStrength);

    //vec3 lightDir = normalize(ubo.lightPos - fragPos);
    vec3 lightDir = normalize(ubo.directionalLightPos);

    vec3 normal = calculateNormal();
    
    vec3 baseColor = vec3(inColor);
    baseColor = texture(baseColorTextureSampler, inTexCoord).rgb;
    vec3 diffuse = max(dot(normal, lightDir), 0.0) * baseColor;

    vec3 viewDir = normalize(ubo.viewPos - inWorldPos);
    vec3 halfDir = normalize(lightDir + viewDir);

    float spec = pow(max(dot(halfDir, normal), 0.0), 5);

    vec3 result = diffuse * vec3(0.5);
    vec3 specular = spec * vec3(0.2);

    result = result + specular + ambient;

    highp vec3 color = result;
    // tone mapping
    //color = Uncharted2Tonemap(color * 4.5f);
    //color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

    outColor = vec4(result, 1.0);
}
