#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 2) uniform UniformBufferDynamicObject
{
    mat4 model;
} uboDynamic;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outWorldPos;
layout(location = 4) out vec3 outTangent;

out gl_PerVertex 
{
    vec4 gl_Position;
};

void main() 
{
    gl_Position = ubo.proj * ubo.view * uboDynamic.model * vec4(inPosition, 1.0);
    outColor = inColor;

    mat3x3 tangent_matrix = mat3x3(uboDynamic.model[0].xyz, uboDynamic.model[1].xyz, uboDynamic.model[2].xyz);
    outNormal            = normalize(tangent_matrix * inNormal);
    outTangent           = normalize(tangent_matrix * inTangent);

    ///outNormal = mat3(transpose(inverse(uboDynamic.model))) * inNormal;
    outTexCoord = inTexCoord;

    outWorldPos = vec3(uboDynamic.model * vec4(inPosition, 1.0));
}
