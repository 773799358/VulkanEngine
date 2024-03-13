#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable

#extension GL_GOOGLE_include_directive : enable

#include "common.h"
#include "DisneyBRDF.h"

layout(location = 0) out highp vec4 outColor;

layout(location = 0) in highp vec2 inTexCoord;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform highp subpassInput in_gbuffer_a;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform highp subpassInput in_gbuffer_b;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform highp subpassInput in_gbuffer_c;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform highp subpassInput in_scene_depth;

//layout(set = 0, binding = 0) uniform UniformBufferObjectPV
//{
//    mat4 view;
//    mat4 proj;
//} projView;

//layout(set = 0, binding = 1) uniform UniformBufferObject
//{
//    vec3 viewPos;
//	float ambientStrength;
//	vec3 directionalLightPos;
//	float padding0;
//	vec3 directionalLightColor;
//    float padding1;
//    mat4x4 directionalLightProjView;
//} ubo;

layout(set = 1, binding = 0) uniform sampler2D directionalLightShadowMapSampler;
layout(set = 1, binding = 1) uniform ShadowProjView
{
    mat4x4 shadowProjView;
} shadowUbo;

void main()
{
    //highp vec3 inWorldPos;
    //{
    //    highp float scene_depth              = subpassLoad(in_scene_depth).r;
    //    highp vec4  ndc                      = vec4(uvToNdcxy(inTexCoord), scene_depth, 1.0);
    //    highp mat4  inverse_proj_view_matrix = inverse(projView.proj * projView.view);
    //    highp vec4  in_world_position_with_w = inverse_proj_view_matrix * ndc;
    //    inWorldPos                           = in_world_position_with_w.xyz / in_world_position_with_w.www;
    //}

    //highp float ambientStrength = ubo.ambientStrength;
    //highp vec3 ambientLight = vec3(ambientStrength);
    //highp vec3 directionalLightDirection = normalize(ubo.directionalLightPos);
    //highp vec3 directionalLightColor = vec3(ubo.directionalLightColor);

    //highp vec3  N = subpassLoad(in_gbuffer_a).rgb * 2.0 - 1.0;

    baseColor = subpassLoad(in_gbuffer_c).rgb;
    metallic = subpassLoad(in_gbuffer_b).z;
    roughness = subpassLoad(in_gbuffer_b).y;
    specular = subpassLoad(in_gbuffer_b).x;

    //highp vec3 V = normalize(ubo.viewPos - inWorldPos);

    // direct light specular and diffuse BRDF contribution
    //highp vec3 Lo = vec3(0.0, 0.0, 0.0);

    // direct ambient contribution
    //highp vec3 La = vec3(0.0f, 0.0f, 0.0f);
    //La            = baseColor * ambientLight;

    // directional light
    //{
    //    highp vec3  L   = normalize(directionalLightDirection);
    //    highp float NoL = min(dot(N, L), 1.0);
//
    //    if (NoL > 0.0)
    //    {
    //        highp float shadow = 0.0;
    //        
    //        shadow = calculateShadow(directionalLightShadowMapSampler, inWorldPos, shadowUbo.shadowProjView);
//
    //        //if (shadow > 0.0f)
    //        {
    //            //highp vec3 En = directionalLightColor * NoL;
    //            //Lo += BRDF(L, V, N, F0, basecolor, metallic, roughness) * En;
    //            Lo += BRDF(L, V, N, T, B) * shadow;
    //        }
    //    }
    //}

    //highp vec3 result = Lo + La;

    //highp vec3 color = result;
    // tone mapping
    //color = toneMapping(color);
    
    // Gamma correct
    // TODO: select the VK_FORMAT_B8G8R8A8_SRGB surface format,
    // there is no need to do gamma correction in the fragment shader
    //color = gamma(color);

    //outColor = vec4(vec3(color), 1.0);

    outColor = subpassLoad(in_scene_depth).rgba;
}