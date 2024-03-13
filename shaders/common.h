
#define PI 3.14159265358979323846

// 平方函数，square
highp float sqr(float x) { return x*x; }

highp vec3 EncodeNormal(highp vec3 N) { return N * 0.5 + 0.5; }

highp vec3 DecodeNormal(highp vec3 N) { return N * 2.0 - 1.0; }

vec3 T;
vec3 B;

highp vec3 calculateNormal(sampler2D normalTex, vec2 uv, vec3 worldPos, vec3 tangent, vec3 normal)
{
    highp vec3 tangent_normal = texture(normalTex, uv).xyz * 2.0 - 1.0;

    highp vec3 N = normalize(normal);
    T = normalize(tangent.xyz);
    if(length(tangent) < 0.0001 || isnan(tangent.x))
    {
        vec3 pos_dx = dFdx(worldPos);
        vec3 pos_dy = dFdy(worldPos);
        vec3 tex_dx = dFdx(vec3(uv, 0.0));
        vec3 tex_dy = dFdy(vec3(uv, 0.0));
        T = (tex_dy.t * pos_dx - tex_dx.t * pos_dy) / (tex_dx.s * tex_dy.t - tex_dy.s * tex_dx.t);
    }
    B = normalize(cross(N, T));

    highp mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangent_normal);
}

highp vec3 Uncharted2Tonemap(highp vec3 x)
{
    highp float A = 0.15;
    highp float B = 0.50;
    highp float C = 0.10;
    highp float D = 0.20;
    highp float E = 0.02;
    highp float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 toneMapping(vec3 color)
{
    vec3 ret;
    ret = Uncharted2Tonemap(color * 4.5f);
    ret = ret * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    return ret;
}

vec3 gamma(vec3 color)
{
    return vec3(pow(color.x, 1.0 / 2.2), pow(color.y, 1.0 / 2.2), pow(color.z, 1.0 / 2.2));
}

highp vec2 ndcxyToUv(highp vec2 ndcxy) { return ndcxy * vec2(0.5, 0.5) + vec2(0.5, 0.5); }

highp vec2 uvToNdcxy(highp vec2 uv) { return uv * vec2(2.0, 2.0) + vec2(-1.0, -1.0); }

float calculateShadow(sampler2D shadowMap, vec3 worldPos, mat4 shadowProjView)
{
    float shadow = 0.0;
    highp vec4 positionClip = shadowProjView * vec4(worldPos, 1.0);
    highp vec3 positionNdc  = positionClip.xyz / positionClip.w;
    highp vec2 uv = ndcxyToUv(positionNdc.xy);
    // PCF
    ivec2 texDim = textureSize(shadowMap, 0);
    float scale = 1.5;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);
    float shadowFactor = 0.0;
    int count = 0;
    int r = 3;

    for (int x = -r; x <= r; x++)
    {
        for (int y = -r; y <= r; y++)
        {
            highp float closestDepth = texture(shadowMap, uv + vec2(dx * float(x), dy * float(y))).x + 0.003;
            highp float currentDepth = positionNdc.z;
            highp float tempShadow = (closestDepth >= currentDepth) ? 1.0f : 0.0f;
            shadowFactor += tempShadow;
            count++;
        }
    
        shadow = shadowFactor / float(count);
    }

    return shadow;
}