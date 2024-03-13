#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable

#include "common.h"
#include "DisneyBRDF.h"

layout(set = 0, binding = 1) uniform UniformBufferObject
{
    vec3 viewPos;
	float ambientStrength;
	vec3 directionalLightPos;
	float padding0;
	vec3 directionalLightColor;
    float padding1;
    mat4x4 directionalLightProjView;
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
    highp float ambientStrength = ubo.ambientStrength;
    highp vec3 ambientLight = vec3(ambientStrength);
    highp vec3 directionalLightDirection = normalize(ubo.directionalLightPos);
    highp vec3 directionalLightColor = vec3(ubo.directionalLightColor);

    highp vec3  N       = calculateNormal(normalTextureSampler, inTexCoord, inWorldPos, inTangent, inNormal);
    if(length(N) < 0.00001 || isnan(N.x))
    {
        N = inNormal;
        T = vec3(1.0, 0.0, 0.0);
        B = vec3(0.0, 1.0, 0.0);
    }

    baseColor           = texture(baseColorTextureSampler, inTexCoord).xyz;
    metallic            = texture(metallicRoughnessTextureSampler, inTexCoord).z;
    roughness           = texture(metallicRoughnessTextureSampler, inTexCoord).y;

    highp vec3 V = normalize(ubo.viewPos - inWorldPos);

    // direct light specular and diffuse BRDF contribution
    highp vec3 Lo = vec3(0.0, 0.0, 0.0);

    // direct ambient contribution
    highp vec3 La = vec3(0.0f, 0.0f, 0.0f);
    La            = baseColor * ambientLight;

    // directional light
    {
        highp vec3  L   = normalize(directionalLightDirection);
        highp float NoL = min(dot(N, L), 1.0);

        if (NoL > 0.0)
        {
            highp float shadow = 0.0;
            
            shadow = calculateShadow(directionalLightShadowMapSampler, inWorldPos, shadowUbo.shadowProjView);

            //if (shadow > 0.0f)
            {
                //highp vec3 En = directionalLightColor * NoL;
                //Lo += BRDF(L, V, N, F0, basecolor, metallic, roughness) * En;
                Lo += BRDF(L, V, N, T, B) * shadow;
            }
        }
    }

    highp vec3 result = Lo + La;

    highp vec3 color = result;
    // tone mapping
    color = toneMapping(color);
    
    // Gamma correct
    // TODO: select the VK_FORMAT_B8G8R8A8_SRGB surface format,
    // there is no need to do gamma correction in the fragment shader
    color = gamma(color);

    outColor = vec4(vec3(color), 1.0);
}
