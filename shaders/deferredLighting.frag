#version 310 es

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out highp vec4 out_Color;

layout(location = 0) in highp vec2 inTexCoord;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform highp subpassInput in_gbuffer_a;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform highp subpassInput in_gbuffer_b;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform highp subpassInput in_gbuffer_c;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform highp subpassInput in_scene_depth;

void main()
{
    out_Color = subpassLoad(in_gbuffer_c).rgba;
}