#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_OES_standard_derivatives : enable
#extension GL_EXT_shader_texture_lod: enable

#include "common.h"

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

layout(set = 2, binding = 0) uniform sampler2D directionalLightShadowMapSampler;
layout(set = 2, binding = 1) uniform ShadowProjView
{
    mat4x4 shadowProjView;
} shadowUbo;

// layout(location = 0)修饰符明确framebuffer的索引
layout(location = 0) out highp vec4 outColor;

void main() 
{
    vec3 ambient = vec3(ubo.ambientStrength);

    //vec3 lightDir = normalize(ubo.lightPos - fragPos);
    vec3 lightDir = normalize(ubo.directionalLightPos);

    vec3 normal = calculateNormal(normalTextureSampler, inTexCoord, inWorldPos, inTangent, inNormal);
    
    vec3 baseColor = vec3(inColor);
    baseColor = texture(baseColorTextureSampler, inTexCoord).rgb;
    vec3 diffuse = max(dot(normal, lightDir), 0.0) * baseColor;

    vec3 viewDir = normalize(ubo.viewPos - inWorldPos);
    vec3 halfDir = normalize(lightDir + viewDir);

    float spec = pow(max(dot(halfDir, normal), 0.0), 5);

    highp float shadow = 0.0;
            
    shadow = calculateShadow(directionalLightShadowMapSampler, inWorldPos, shadowUbo.shadowProjView);

    vec3 result = diffuse * vec3(0.5);
    vec3 specular = spec * vec3(0.2);

    result = (result + specular) * shadow + ambient;

    highp vec3 color = result;

    outColor = vec4(result, 1.0);
}
