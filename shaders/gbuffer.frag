#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable

#extension GL_GOOGLE_include_directive : enable

#include "common.h"

layout(location = 0) out highp vec4 out_gbuffer0;
layout(location = 1) out highp vec4 out_gbuffer1;
layout(location = 2) out highp vec4 out_gbuffer2;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inWorldPos;
layout(location = 4) in vec3 inTangent;

layout(set = 1, binding = 0) uniform sampler2D baseColorTextureSampler;
layout(set = 1, binding = 1) uniform sampler2D normalTextureSampler;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTextureSampler;

void main()
{
    vec3 worldNormal = calculateNormal(normalTextureSampler, inTexCoord, inWorldPos, inTangent, inNormal);
    vec3 albedo = texture(baseColorTextureSampler, inTexCoord).xyz;
    float metalic = texture(metallicRoughnessTextureSampler, inTexCoord).z;
    float specular = 0.5;
    float roughness = texture(metallicRoughnessTextureSampler, inTexCoord).y;
    float shadingModelID = 1.0;

    out_gbuffer0 = vec4(EncodeNormal(worldNormal), 1.0);
    out_gbuffer1 = vec4(metalic, specular, roughness, shadingModelID / 255.0);
    out_gbuffer2 = vec4(albedo, 1.0);
}