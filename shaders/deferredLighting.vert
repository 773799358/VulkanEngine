#version 310 es

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec2 out_texcoord;

vec2 positions[3] = vec2[]
(
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

void main()
{
    vec3 fullscreen_triangle_positions[3] = vec3[3](vec3(3.0, 1.0, 0.5), vec3(-1.0, 1.0, 0.5), vec3(-1.0, -3.0, 0.5));

    vec2 fullscreen_triangle_uvs[3] = vec2[3](vec2(2.0, 1.0), vec2(0.0, 1.0), vec2(0.0, -1.0));

    gl_Position  = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    out_texcoord = fullscreen_triangle_uvs[gl_VertexIndex];
}