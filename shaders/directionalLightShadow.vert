#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 projView;
} ubo;

layout(set = 0, binding = 1) uniform UniformBufferDynamicObject
{
    mat4 model;
} uboDynamic;

layout(location = 0) in vec3 inPosition;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = ubo.projView * uboDynamic.model * vec4(inPosition, 1.0);
}