#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform UniformBufferObject
{
    vec3 baseLightColor;
    float ambientStrength;
    vec3 lightPos;
    float specularStrength;
    vec3 viewPos;
} ubo;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragPos;

layout(binding = 2) uniform sampler2D texSampler;

// layout(location = 0)修饰符明确framebuffer的索引
layout(location = 0) out vec4 outColor;

void main() 
{
    vec3 ambient = vec3(ubo.ambientStrength);

    //vec3 lightDir = normalize(ubo.lightPos - fragPos);
    vec3 lightDir = normalize(vec3(ubo.lightPos));

    vec3 normal = normalize(fragNormal);
    
    vec3 color = vec3(1.0);
    //color = texture(texSampler, fragTexCoord).rgb;
    vec3 diffuse = max(dot(normal, lightDir), 0.0) * color;

    vec3 viewDir = normalize(ubo.viewPos - fragPos);
    vec3 halfDir = normalize(lightDir + viewDir);

    float spec = pow(max(dot(halfDir, normal), 0.0), 8);

    vec3 result = diffuse * ubo.baseLightColor;
    vec3 specular = spec * vec3(ubo.specularStrength);

    result = result + specular + ambient;

    outColor = vec4(result, 1.0);
}
