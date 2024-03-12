#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_texture_lod: enable
#extension GL_OES_standard_derivatives : enable

#define PI 3.14159265358979323846

layout(set = 0, binding = 1) uniform UniformBufferObject
{
    vec3 viewPos;
	float ambientStrength;
	vec3 directionalLightPos;
	float padding0;
	vec3 directionalLightColor;
    float padding1;
    mat4x4 directionalLightProjView;
} ubo;

layout(location = 0) in highp vec3 inColor;
layout(location = 1) in highp vec3 inNormal;
layout(location = 2) in highp vec2 inTexCoord;
layout(location = 3) in highp vec3 inWorldPos;
layout(location = 4) in highp vec3 inTangent;

layout(set = 1, binding = 0) uniform sampler2D baseColorTextureSampler;
layout(set = 1, binding = 1) uniform sampler2D normalTextureSampler;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTextureSampler;

// layout(location = 0)修饰符明确framebuffer的索引
layout(location = 0) out highp vec4 outColor;

// 平方函数，square
float sqr(float x) { return x*x; }

// 入参u时视方向与法线的点积
// 返回F0为0情况下的SchlickFresnel的解，既0 + (1-0)(1-vdoth)^5
float SchlickFresnel(float u)
{
    float m = clamp(1-u, 0, 1);// 将1-vdoth的结果限制在[0,1]空间内 
    float m2 = m*m;
    return m2*m2*m; //  pow(m,5) 求5次方
}

// GTR 1 (次镜面波瓣，gamma=1)
// 次用于上层透明涂层材质（ClearCoat清漆材质，俗称上层高光），是各向同性且非金属的。
float GTR1(float NdotH, float a)
{
    // 考虑到粗糙度a在等于1的情况下，公式返回值无意义，因此固定返回1/pi，
    // 说明在完全粗糙的情况下，各个方向的法线分布均匀，且积分后得1
    if (a >= 1) return 1/PI;

    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH; // 公式主部
    return (a2-1) / (PI*log(a2)*t);   // GTR1的c，迪士尼取值为：(a2-1)/(PI*log(a2))
}

// GTR 2(主镜面波瓣，gamma=2) 先用于基础层材质（Base Material）用于各项同性的金属或非金属（俗称下层高光）
float GTR2(float NdotH, float a)
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t*t);  // GTR2的c，迪士尼取值为:a2/PI
}

// 主镜面波瓣的各向异性版本
// 其中HdotX为半角向量与物体表面法线空间中的切线方向向量的点积
// HdotY为半角点乘切线空间中的副切线向量 
// ax 和 ay 分别是这2个方向上的可感粗糙度，范围是0~1 
float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1 / (PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}

// 2012版disney采用的Smith GGX导出的G项，本质是Smith联合遮蔽阴影函数中的“分离的遮蔽阴影型”
// 入参NdotV视情况也可替换会NdotL，用于计算阴影相关G1；
// 入参alphaG被迪士尼定义为0.25f
// 该公式各项同性，迪士尼使用此公式计算第二高光波瓣 
float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1 / (NdotV + sqrt(a + b - a*b));
}

// 各向异性版G 
float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1 / (NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ));
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

vec3 baseColor = vec3(0.82, 0.67, 0.16);

float metallic = 0;
float subsurface = 0;
float specular = 0.5;
float roughness = 0.5;
float specularTint = 0;
float anisotropic = 0;
float sheen = 0;
float sheenTint = 0.5;
float clearcoat = 0;
float clearcoatGloss = 1;

vec3 BRDF( vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y )
{
    // 准备参数
    float NdotL = dot(N,L);
    float NdotV = dot(N,V);
    if (NdotL < 0 || NdotV < 0) return vec3(0); // 无视水平面以下的光线或视线 
    vec3 H = normalize(L+V);                    // 半角向量
    float NdotH = dot(N,H);
    float LdotH = dot(L,H);

    vec3 Cdlin = mon2lin(baseColor);                        // 将gamma空间的颜色转换到线性空间，目前存储的还是rgb
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx. 近似的将rgb转换成光亮度 luminance 

    // 对baseColor按亮度归一化，从而独立出色调和饱和度，可以认为Ctint是与亮度无关的固有色调 
    vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); //  normalize lum. to isolate hue+sat
    // 混淆得到高光底色，包含2次mix操作
    // 第一次从白色按照用户输入的specularTint插值到Ctint。列如specularTint为0，则返回纯白色
    // 第二次从第一次混淆的返回值开始，按照金属度metallic，插值到带有亮度的线性空间baseColor。
    // 列如金属度为1，则返回本色baseColor。如果金属度为0，既电介质，那么返回第一次插值的结果得一定比例（0.8*specular倍）
    vec3 Cspec0 = mix(specular*.08*mix(vec3(1), Ctint, specularTint), Cdlin, metallic);
    // 这是光泽颜色，我们知道光泽度用于补偿布料等材质在FresnelPeak处的额外光能，光泽颜色则从白色开始，按照用户输入的sheenTint值，插值到Ctint为止。
    vec3 Csheen = mix(vec3(1), Ctint, sheenTint);

    // 以下代码段负责计算Diffuse分量 
    //  Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    //  and mix in diffuse retro-reflection based on roughness
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV); // SchlickFresnel返回的是(1-cosθ)^5的计算结果 
    float Fd90 = 0.5 + 2 * LdotH*LdotH * roughness; // 使用粗糙度计算漫反射的反射率
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);  // 此步骤还没有乘以baseColor/pi，会在当前代码段之外完成。

    // 以下代码负责计算SS(次表面散射)分量 
    //  Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf（基于各向同性bssrdf的Hanrahan-Krueger brdf逼近）
    //  1.25 scale is used to (roughly) preserve albedo（1.25的缩放是用于（大致）保留反照率）
    //  Fss90 used to "flatten" retroreflection based on roughness （Fss90用于“平整”基于粗糙度的逆反射）
    float Fss90 = LdotH*LdotH*roughness; // 垂直于次表面的菲涅尔系数
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5); // 此步骤同样还没有乘以baseColor/pi，会在当前代码段之外完成。

    //  specular
    float aspect = sqrt(1-anisotropic*.9);// aspect 将艺术家手中的anisotropic参数重映射到[0.1,1]空间，确保aspect不为0,
    float ax = max(.001, sqr(roughness)/aspect);                    // ax随着参数anisotropic的增加而增加
    float ay = max(.001, sqr(roughness)*aspect);                    // ay随着参数anisotropic的增加而减少，ax和ay在anisotropic值为0时相等
    float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);  // 各项异性GTR2导出对应H的法线强度
    float FH = SchlickFresnel(LdotH);  // 返回菲尼尔核心，既pow(1-cosθd,5)
    vec3 Fs = mix(Cspec0, vec3(1), FH); // 菲尼尔项，使用了Cspec0作为F0，既高光底色，模拟金属的菲涅尔染色 
    float Gs;   // 几何项，一般与l，v和n相关，各项异性时还需要考虑方向空间中的切线t和副切线b
    Gs  = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);  // 遮蔽关联的几何项G1
    Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay); // 阴影关联的几何项G1，随后合并两项存入Gs

    //  sheen 光泽项，本身作为边缘处漫反射的补偿 
    vec3 Fsheen = FH * sheen * Csheen; // 迪士尼认为sheen值正比于菲涅尔项FH，同时强度被控制变量sheen和颜色控制变量Csheen影响 

    //  clearcoat (ior = 1.5 -> F0 = 0.04)
    // 清漆层没有漫反射，只有镜面反射，使用独立的D,F和G项 
    // 下面行使用GTR1（berry）分布函数获取法线强度，第二个参数是a（粗糙度）
    // 迪士尼使用用户控制变量clearcoatGloss，在0.1到0.001线性插值获取a 
    float Dr = GTR1(NdotH, mix(.1,.001,clearcoatGloss)); 
    float Fr = mix(.04, 1.0, FH);  // 菲涅尔项上调最低值至0.04 
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);    // 几何项使用各项同性的smithG_GGX计算，a固定给0.25 

    // （漫反射 + 光泽）* 非金属度 + 镜面反射 + 清漆高光
    //  注意漫反射计算时使用了subsurface控制变量对基于菲涅尔的漫反射 和 次表面散射进行插值过渡；此外还补上了之前提到的baseColor/pi
    //  使用非金属度（既：1-金属度）用以消除来自金属的漫反射 <- 非物理，但是能插值很爽啊 
    return ((1/PI) * mix(Fd, ss, subsurface)*Cdlin + Fsheen) 
        * (1-metallic)
        + Gs*Fs*Ds + .25*clearcoat*Gr*Fr*Dr;
}

vec3 T;
vec3 B;

highp vec3 calculateNormal()
{
    highp vec3 tangent_normal = texture(normalTextureSampler, inTexCoord).xyz * 2.0 - 1.0;

    highp vec3 N = normalize(inNormal);
    T = normalize(inTangent.xyz);
    if(length(inTangent) < 0.0001 || isnan(inTangent.x))
    {
        vec3 pos_dx = dFdx(inWorldPos);
        vec3 pos_dy = dFdy(inWorldPos);
        vec3 tex_dx = dFdx(vec3(inTexCoord, 0.0));
        vec3 tex_dy = dFdy(vec3(inTexCoord, 0.0));
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

void main() 
{
    highp float ambientStrength = ubo.ambientStrength;
    highp vec3 ambientLight = vec3(ambientStrength);
    highp vec3 directionalLightDirection = normalize(ubo.directionalLightPos);
    highp vec3 directionalLightColor = vec3(ubo.directionalLightColor);

    highp vec3  N       = calculateNormal();
    if(length(N) < 0.00001 || isnan(N.x))
    {
        N = inNormal;
        T = vec3(1.0, 0.0, 0.0);
        B = vec3(0.0, 1.0, 0.0);
    }

    baseColor           = texture(baseColorTextureSampler, inTexCoord).xyz;
    metallic            = texture(metallicRoughnessTextureSampler, inTexCoord).z;
    roughness           = texture(metallicRoughnessTextureSampler, inTexCoord).y;

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
            //highp float shadow;
            //{
            //    highp vec4 position_clip = directional_light_proj_view * vec4(in_world_position, 1.0);
            //    highp vec3 position_ndc  = position_clip.xyz / position_clip.w;

            //    highp vec2 uv = ndcxy_to_uv(position_ndc.xy);

            //    highp float closest_depth = texture(directional_light_shadow, uv).r + 0.000075;
            //    highp float current_depth = position_ndc.z;

            //    shadow = (closest_depth >= current_depth) ? 1.0f : -1.0f;
            //}

            //if (shadow > 0.0f)
            {
                //highp vec3 En = directionalLightColor * NoL;
                //Lo += BRDF(L, V, N, F0, basecolor, metallic, roughness) * En;
                Lo += BRDF(L, V, N, T, B);
            }
        }
    }

    highp vec3 result = Lo + La;

    highp vec3 color = result;
    // tone mapping
    color = Uncharted2Tonemap(color * 4.5f);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

    // Gamma correct
    // TODO: select the VK_FORMAT_B8G8R8A8_SRGB surface format,
    // there is no need to do gamma correction in the fragment shader
    color = vec3(pow(color.x, 1.0 / 2.2), pow(color.y, 1.0 / 2.2), pow(color.z, 1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
