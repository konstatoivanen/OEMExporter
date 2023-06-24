#pragma once

// @TODO split these up to reuasble chunks.

#define shader_compute_common                                                                   \
"#version 450 core																		\n"     \
"uniform float _Roughness;																\n"     \
"uniform uvec2 _Offset;																	\n"     \
"uniform sampler2D _Source;																\n"     \
"#define SAMPLE_COUNT 200000u															\n"     \
"																						\n"     \
"vec3 OctaDecode(vec2 f)																\n"     \
"{																						\n"     \
"    f = f * 2.0f - 1.0f;																\n"     \
"    vec3 n = vec3(f.x, 1.0f - abs(f.x) - abs(f.y), f.y);								\n"     \
"    float t = max(-n.y, 0.0);															\n"     \
"    n.x += n.x >= 0.0f ? -t : t;														\n"     \
"    n.z += n.z >= 0.0f ? -t : t;														\n"     \
"    return normalize(n);																\n"     \
"}																						\n"     \
"																						\n"     \
"vec2 CylinderUV(vec3 direction)														\n"     \
"{																						\n"     \
"    float angleh = (atan(direction.x, direction.z) + 3.14159265359f) * 0.15915494309f;	\n"     \
"    float anglev = acos(dot(direction, vec3(0, 1, 0))) * 0.31830988618f;				\n"     \
"    return vec2(angleh, anglev);														\n"     \
"}																						\n"     \
"																						\n"     \
"float RadicalInverse_VdC(uint bits)													\n"     \
"{																						\n"     \
"    bits = (bits << 16u) | (bits >> 16u);												\n"     \
"    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);				\n"     \
"    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);				\n"     \
"    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);				\n"     \
"    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);				\n"     \
"    return float(bits) * 2.3283064365386963e-10;										\n"     \
"}																						\n"     \
"																						\n"     \
"vec2 Hammersley(uint i, uint N)														\n"     \
"{																						\n"     \
"    return vec2(float(i) / float(N), RadicalInverse_VdC(i));							\n"     \
"}																						\n"     \
"																						\n"     \
"vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)								\n"     \
"{																						\n"     \
"    float a = roughness * roughness;													\n"     \
"																						\n"     \
"    float phi = 2.0 * 3.14159265 * Xi.x;												\n"     \
"    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));				\n"     \
"    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);									\n"     \
"																						\n"     \
"    vec3 H;																			\n"     \
"    H.x = cos(phi) * sinTheta;															\n"     \
"    H.y = sin(phi) * sinTheta;															\n"     \
"    H.z = cosTheta;																	\n"     \
"																						\n"     \
"    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);			\n"     \
"    vec3 tangent = normalize(cross(up, N));											\n"     \
"    vec3 bitangent = cross(N, tangent);												\n"     \
"    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;						\n"     \
"    return normalize(sampleVec);														\n"     \
"}																						\n"     \
"																						\n"     \
"                                                                                       \n"     \
"vec3 Filter(vec2 size, ivec2 coord)                                                    \n"     \
"{                                                                                      \n"     \
"   vec2 uv = (coord + 0.5f.xx) / size;													\n"     \
"   vec3 N = OctaDecode(uv);															\n"     \
"   vec3 R = N;			    															\n"     \
"   vec3 V = R;																			\n"     \
"   																					\n"     \
"   float totalWeight = 0.0;															\n"     \
"   vec3 prefilteredColor = 0.0f.xxx;													\n"     \
"   																					\n"     \
"   for (uint i = 0u; i < SAMPLE_COUNT; ++i)											\n"     \
"   {																					\n"     \
"       vec2 Xi = Hammersley(i, SAMPLE_COUNT);											\n"     \
"       vec3 H = ImportanceSampleGGX(Xi, N, _Roughness);								\n"     \
"       vec3 L = normalize(2.0 * dot(V, H) * H - V);									\n"     \
"   																					\n"     \
"       float NdotL = max(dot(N, L), 0.0);												\n"     \
"   																					\n"     \
"       if (NdotL > 0.0)																\n"     \
"       {																				\n"     \
"           vec2 cylinderuv = CylinderUV(L);											\n"     \
"           vec3 env = texture(_Source, cylinderuv).rgb;								\n"     \
"           prefilteredColor += env * NdotL;											\n"     \
"           totalWeight += NdotL;														\n"     \
"       }																				\n"     \
"   }																					\n"     \
"   																					\n"     \
"   prefilteredColor /= totalWeight;													\n"     \
"   return prefilteredColor;                                                            \n"     \
"}                                                                                      \n"     \


constexpr const static char* shader_compute_rgbm = 
shader_compute_common
"layout(rgba8) uniform writeonly image2D _WriteTarget;							\n"
"																				\n"
"#define HDRFactor 8.0															\n"
"vec4 HDREncode(vec3 color)														\n"
"{																				\n"
"    color /= HDRFactor;														\n"
"    float alpha = ceil(max(max(color.r, color.g), color.b) * 255.0) / 255.0;	\n"
"    return vec4(color / alpha, alpha);											\n"
"}																				\n"
"																				\n"
"layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;				\n"
"void main()																	\n"
"{																				\n"
"    vec2 size = vec2(imageSize(_WriteTarget).xy);								\n"
"    ivec2 coord = ivec2(gl_GlobalInvocationID.xy + _Offset);					\n"
"    																			\n"
"    if (coord.x < size.x && coord.y < size.y)									\n"
"    {																			\n"
"       vec3 prefilteredColor = Filter(size, coord);                            \n"
"       imageStore(_WriteTarget, coord, HDREncode(prefilteredColor));			\n"
"    }																			\n"
"}																				\n";

//Source: https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
constexpr const static char* shader_compute_rgb9e5 =
shader_compute_common
"layout(r32ui) uniform writeonly uimage2D _WriteTarget;							\n"
"																				\n"
"#define ENCODE_E5BGR9_EXPONENT_BITS 5                                          \n"
"#define ENCODE_E5BGR9_MANTISSA_BITS 9                                          \n"
"#define ENCODE_E5BGR9_MAX_VALID_BIASED_EXP 31                                  \n"
"#define ENCODE_E5BGR9_EXP_BIAS 15                                              \n"
"#define ENCODE_E5BGR9_MANTISSA_VALUES (1 << 9)                                 \n"
"#define ENCODE_E5BGR9_MANTISSA_MASK (ENCODE_E5BGR9_MANTISSA_VALUES - 1)        \n"
"#define ENCODE_E5BGR9_SHAREDEXP_MAX 65408                                      \n"
"                                                                               \n"
"uint EncodeE5BGR9(vec3 unpacked)                                               \n"
"{                                                                              \n"
"    const int N = ENCODE_E5BGR9_MANTISSA_BITS;                                 \n"
"    const int Np2 = 1 << N;                                                    \n"
"    const int B = ENCODE_E5BGR9_EXP_BIAS;                                      \n"
"                                                                               \n"
"    unpacked = clamp(unpacked, 0.0f.xxx, ENCODE_E5BGR9_SHAREDEXP_MAX.xxx);     \n"
"    float max_c = max(unpacked.r, max(unpacked.g, unpacked.b));                \n"
"                                                                               \n"
"    // for log2                                                                \n"
"    if (max_c == 0.0)                                                          \n"
"    {                                                                          \n"
"        return 0;                                                              \n"
"    }                                                                          \n"
"                                                                               \n"
"    int exp_shared_p = max(-B-1, int(floor(log2(max_c)))) + 1 + B;             \n"
"    int max_s = int(round(max_c * exp2(-float(exp_shared_p - B - N))));        \n"
"                                                                               \n"
"    int exp_shared = max_s != Np2 ?                                            \n"
"        exp_shared_p :                                                         \n"
"        exp_shared_p + 1;                                                      \n"
"                                                                               \n"
"    float s = exp2(-float(exp_shared - B - N));                                \n"
"    uvec3 rgb_s = uvec3(round(unpacked * s));                                  \n"
"                                                                               \n"
"    return                                                                     \n"
"        (exp_shared << (3 * ENCODE_E5BGR9_MANTISSA_BITS)) |                    \n"
"        (rgb_s.b    << (2 * ENCODE_E5BGR9_MANTISSA_BITS)) |                    \n"
"        (rgb_s.g    << (1 * ENCODE_E5BGR9_MANTISSA_BITS)) |                    \n"
"        (rgb_s.r);                                                             \n"
"}                                                                              \n"
"                                                                               \n"
"layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;				\n"
"void main()																	\n"
"{																				\n"
"    vec2 size = vec2(imageSize(_WriteTarget).xy);								\n"
"    ivec2 coord = ivec2(gl_GlobalInvocationID.xy + _Offset);					\n"
"    																			\n"
"    if (coord.x < size.x && coord.y < size.y)									\n"
"    {																			\n"
"       vec3 prefilteredColor = Filter(size, coord);                            \n"
"       imageStore(_WriteTarget, coord, uvec4(EncodeE5BGR9(prefilteredColor)));	\n"
"    }																			\n"
"}																				\n";


constexpr const static char* shader_compute_rgb16f =
shader_compute_common
"layout(rgba16f) uniform writeonly image2D _WriteTarget;				\n"
"																		\n"
"layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;		\n"
"void main()															\n"
"{																		\n"
"    vec2 size = vec2(imageSize(_WriteTarget).xy);						\n"
"    ivec2 coord = ivec2(gl_GlobalInvocationID.xy + _Offset);			\n"
"    																	\n"
"    if (coord.x < size.x && coord.y < size.y)							\n"
"    {																	\n"
"       vec3 prefilteredColor = Filter(size, coord);                    \n"
"       imageStore(_WriteTarget, coord, vec4(prefilteredColor, 1.0f));	\n"
"    }																	\n"
"}																		\n";

constexpr const static char* shader_vertex =
" #version 450 core														\n"
" 																		\n"
" out vec2 vs_TEXCOORD0;												\n"
" 																		\n"
" void main()															\n"
" {																		\n"
"     vs_TEXCOORD0 = vec2( (gl_VertexID << 1) & 2, gl_VertexID & 2 );	\n"
"     gl_Position = vec4(vs_TEXCOORD0 * 2.0 - 1.0, 0.0, 1.0 );			\n"
" }																		\n";

constexpr const static char* shader_fragment =
"#version 450 core															\n"
"																			\n"
"uniform sampler2D _Source;													\n"
"uniform int _Lod;															\n"
"																			\n"
"in vec2 vs_TEXCOORD0;														\n"
"out vec4 SV_Target0;														\n"
"																			\n"
"void main()																\n"
"{																			\n"
"	ivec2 coord = ivec2(vs_TEXCOORD0 * textureSize(_Source, _Lod).xy);		\n"
"	SV_Target0 = vec4(texelFetch(_Source, coord, _Lod).rgb, 1.0f);			\n"
"}																			\n";

constexpr const static char* shader_fragment_rgb9e5 =
"	#version 450 core															            \n"
"																				            \n"
"#define ENCODE_E5BGR9_EXPONENT_BITS 5                                                      \n"
"#define ENCODE_E5BGR9_MANTISSA_BITS 9                                                      \n"
"#define ENCODE_E5BGR9_MAX_VALID_BIASED_EXP 31                                              \n"
"#define ENCODE_E5BGR9_EXP_BIAS 15                                                          \n"
"#define ENCODE_E5BGR9_MANTISSA_VALUES (1 << 9)                                             \n"
"#define ENCODE_E5BGR9_MANTISSA_MASK (ENCODE_E5BGR9_MANTISSA_VALUES - 1)                    \n"
"#define ENCODE_E5BGR9_SHAREDEXP_MAX 65408                                                  \n"
"																				            \n"
"vec3 DecodeE5BGR9(const uint _packed)                                                      \n"
"{                                                                                          \n"
"    const int N = ENCODE_E5BGR9_MANTISSA_BITS;                                             \n"
"    const int B = ENCODE_E5BGR9_EXP_BIAS;                                                  \n"
"                                                                                           \n"
"    int exp_shared = int(_packed >> (3 * ENCODE_E5BGR9_MANTISSA_BITS));                    \n"
"    float s = exp2(float(exp_shared - B - N));                                             \n"
"                                                                                           \n"
"    return s * vec3(                                                                       \n"
"        (_packed                                       ) & ENCODE_E5BGR9_MANTISSA_MASK,    \n"
"        (_packed >> (1 * ENCODE_E5BGR9_MANTISSA_BITS)) & ENCODE_E5BGR9_MANTISSA_MASK,      \n"
"        (_packed >> (2 * ENCODE_E5BGR9_MANTISSA_BITS)) & ENCODE_E5BGR9_MANTISSA_MASK       \n"
"    );                                                                                     \n"
"}																				            \n"
"																				            \n"
"uniform usampler2D _Source;													            \n"
"uniform int _Lod;															                \n"
"																			                \n"
"in vec2 vs_TEXCOORD0;														                \n"
"out vec4 SV_Target0;														                \n"
"																			                \n"
"void main()																	            \n"
"{																			                \n"
"	ivec2 coord = ivec2(vs_TEXCOORD0 * textureSize(_Source, _Lod).xy);		                \n"
"	SV_Target0 = vec4(DecodeE5BGR9(texelFetch(_Source, coord, _Lod).r), 1.0f);			    \n"
"}																			                \n";
