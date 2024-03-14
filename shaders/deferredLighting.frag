#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable

#extension GL_GOOGLE_include_directive : enable

#include "common.h"
#include "DisneyBRDF.h"

layout(location = 0) out highp vec4 outColor;

layout(location = 0) in highp vec2 inTexCoord;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform highp subpassInput inGbufferNormal;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform highp subpassInput inGbufferMR;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform highp subpassInput inGbufferAlbedo;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform highp subpassInput inSceneDepth;

layout(set = 2, binding = 0) uniform UniformBufferObject
{
    mat4x4 projView;
    vec3 viewPos;
	float ambientStrength;
	vec3 directionalLightPos;
	float padding0;
	vec3 directionalLightColor;
    float padding1;
    mat4x4 directionalLightProjView;
} ubo;

layout(set = 0, binding = 0) uniform sampler2D directionalLightShadowMapSampler;
layout(set = 0, binding = 1) uniform ShadowProjView
{
    mat4x4 shadowProjView;
} shadowUbo;

void main()
{
    highp vec3 inWorldPos;
    {
        highp float sceneDepth               = subpassLoad(inSceneDepth).r;
        highp vec4  ndc                      = vec4(uvToNdcxy(inTexCoord), sceneDepth, 1.0);
        highp mat4  inverseProjViewMatrix    = inverse(ubo.projView);
        highp vec4  inWorldPositionWithW     = inverseProjViewMatrix * ndc;
        inWorldPos                           = inWorldPositionWithW.xyz / inWorldPositionWithW.www;
    }

    highp float ambientStrength = ubo.ambientStrength;
    highp vec3 ambientLight = vec3(ambientStrength);
    highp vec3 directionalLightDirection = normalize(ubo.directionalLightPos);
    highp vec3 directionalLightColor = vec3(ubo.directionalLightColor);

    highp vec3 N = subpassLoad(inGbufferNormal).rgb * 2.0 - 1.0;
    T = vec3(subpassLoad(inGbufferNormal).a, subpassLoad(inGbufferMR).a, subpassLoad(inGbufferAlbedo).a);
    T = T * 2.0 - 1.0;
    B = normalize(cross(N, T));

    baseColor = subpassLoad(inGbufferAlbedo).rgb;
    metallic = subpassLoad(inGbufferMR).r;
    roughness = subpassLoad(inGbufferMR).b;
    specular = subpassLoad(inGbufferMR).g;

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

    if(subpassLoad(inSceneDepth).r == 1.0)
    {
        color = vec3(0.2);
    }

    outColor = vec4(color, 1.0);
}