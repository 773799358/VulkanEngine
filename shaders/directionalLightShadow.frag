#version 310 es

layout(early_fragment_tests) in;    // 优化，告诉放心做early z；

layout(location = 0) out highp float out_depth;

void main()
{
    out_depth = gl_FragCoord.z; // gl_FragCoord，内置变量，获取屏幕坐标
}