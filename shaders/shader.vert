#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragPos;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    // 内置的gl_VertexIndex变量包含了当前顶点的索引信息
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragTexCoord = inTexCoord;

    fragPos = vec3(ubo.model * vec4(inPosition, 1.0));
}
