#version 450
#extension GL_ARB_separate_shader_objects : enable

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

// Normal Distribution function --------------------------------------
highp float D_GGX(highp float dotNH, highp float roughness)
{
    highp float alpha  = roughness * roughness;
    highp float alpha2 = alpha * alpha;
    highp float denom  = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom);
}

// Geometric Shadowing function --------------------------------------
highp float G_SchlicksmithGGX(highp float dotNL, highp float dotNV, highp float roughness)
{
    highp float r  = (roughness + 1.0);
    highp float k  = (r * r) / 8.0;
    highp float GL = dotNL / (dotNL * (1.0 - k) + k);
    highp float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// Fresnel function ----------------------------------------------------
highp float Pow5(highp float x)
{
    return (x * x * x * x * x);
}

highp vec3 F_Schlick(highp float cosTheta, highp vec3 F0) 
{ 
    return F0 + (1.0 - F0) * Pow5(1.0 - cosTheta); 
    }

highp vec3 F_SchlickR(highp float cosTheta, highp vec3 F0, highp float roughness)
{
    return F0 + (max(vec3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * Pow5(1.0 - cosTheta);
}

// Specular and diffuse BRDF composition --------------------------------------------
highp vec3 BRDF(highp vec3  L,
                highp vec3  V,
                highp vec3  N,
                highp vec3  F0,
                highp vec3  basecolor,
                highp float metallic,
                highp float roughness)
{
    // Precalculate vectors and dot products
    highp vec3  H     = normalize(V + L);
    highp float dotNV = clamp(dot(N, V), 0.0, 1.0);
    highp float dotNL = clamp(dot(N, L), 0.0, 1.0);
    highp float dotLH = clamp(dot(L, H), 0.0, 1.0);
    highp float dotNH = clamp(dot(N, H), 0.0, 1.0);

    // Light color fixed
    // vec3 lightColor = vec3(1.0);

    highp vec3 color = vec3(0.0);

    highp float rroughness = max(0.05, roughness);
    // D = Normal distribution (Distribution of the microfacets)
    highp float D = D_GGX(dotNH, rroughness);
    // G = Geometric shadowing term (Microfacets shadowing)
    highp float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
    // F = Fresnel factor (Reflectance depending on angle of incidence)
    highp vec3 F = F_Schlick(dotNV, F0);

    highp vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
    highp vec3 kD   = (vec3(1.0) - F) * (1.0 - metallic);

    color += (kD * basecolor / PI + (1.0 - kD) * spec);
    // color += (kD * basecolor / PI + spec) * dotNL;
    // color += (kD * basecolor / PI + spec) * dotNL * lightColor;

    return color;
}

highp vec2 ndcxy_to_uv(highp vec2 ndcxy) { return ndcxy * vec2(0.5, 0.5) + vec2(0.5, 0.5); }

highp vec2 uv_to_ndcxy(highp vec2 uv) { return uv * vec2(2.0, 2.0) + vec2(-1.0, -1.0); }

void main() 
{
    highp float ambientStrength = ubo.ambientStrength;
    highp vec3 ambientLight = vec3(ambientStrength);
    highp vec3 directionalLightDirection = normalize(ubo.directionalLightPos);
    highp vec3 directionalLightColor = ubo.directionalLightColor;
    highp float metallicFactor = 1.0;
    highp float roughnessFactor = 1.0;

    highp vec3  N                   = calculateNormal(normalTextureSampler, inTexCoord, inWorldPos, inTangent, inNormal);
    highp vec3  basecolor           = texture(baseColorTextureSampler, inTexCoord).xyz;
    highp float metallic            = texture(metallicRoughnessTextureSampler, inTexCoord).z * metallicFactor;
    highp float dielectricSpecular  = 0.04;
    highp float roughness           = texture(metallicRoughnessTextureSampler, inTexCoord).y * roughnessFactor;

    highp vec3 V = normalize(ubo.viewPos - inWorldPos);
    highp vec3 R = reflect(-V, N);

    highp vec3 originSamplecubeN = vec3(N.x, N.z, N.y);
    highp vec3 originSamplecubeR = vec3(R.x, R.z, R.y);

    highp vec3 F0 = mix(vec3(dielectricSpecular, dielectricSpecular, dielectricSpecular), basecolor, metallic);

    // direct light specular and diffuse BRDF contribution
    highp vec3 Lo = vec3(0.0, 0.0, 0.0);

    // direct ambient contribution
    highp vec3 La = vec3(0.0f, 0.0f, 0.0f);
    La            = basecolor * ambientLight;

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
                highp vec3 En = directionalLightColor * NoL;
                Lo += BRDF(L, V, N, F0, basecolor, metallic, roughness) * shadow * En;
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
    outColor = vec4(color, 1.0);
}
