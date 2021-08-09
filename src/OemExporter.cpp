#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3.h>
#include <GLAD/glad.h>
#include <KTX/ktx.h>

const static char* shader_compute =
"#version 450 core																								\n"
"																												\n"
"uniform float _Roughness;																						\n"
"uniform uvec2 _Offset;																							\n"
"uniform sampler2D _Source;																						\n"
"layout(rgba8) uniform writeonly image2D _WriteTarget;															\n"
"																												\n"
"#define HDRFactor 8.0																							\n"
"#define SAMPLE_COUNT 200000u																					\n"
"																												\n"
"vec4 HDREncode(vec3 color)																						\n"
"{																												\n"
"    color /= HDRFactor;																						\n"
"    float alpha = ceil(max(max(color.r, color.g), color.b) * 255.0) / 255.0;									\n"
"    return vec4(color / alpha, alpha);																			\n"
"}																												\n"
"																												\n"
"vec3 OctaDecode(vec2 f)																						\n"
"{																												\n"
"    f = f * 2.0f - 1.0f;																						\n"
"    vec3 n = vec3(f.x, 1.0f - abs(f.x) - abs(f.y), f.y);														\n"
"    float t = max(-n.y, 0.0);																					\n"
"    n.x += n.x >= 0.0f ? -t : t;																				\n"
"    n.z += n.z >= 0.0f ? -t : t;																				\n"
"    return normalize(n);																						\n"
"}																												\n"
"																												\n"
"vec2 CylinderUV(vec3 direction)																				\n"
"{																												\n"
"    float angleh = (atan(direction.x, direction.z) + 3.14159265359f) * 0.15915494309f;							\n"
"    float anglev = acos(dot(direction, vec3(0, 1, 0))) * 0.31830988618f;										\n"
"    return vec2(angleh, anglev);																				\n"
"}																												\n"
"																												\n"
"float RadicalInverse_VdC(uint bits)																			\n"
"{																												\n"
"    bits = (bits << 16u) | (bits >> 16u);																		\n"
"    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);										\n"
"    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);										\n"
"    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);										\n"
"    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);										\n"
"    return float(bits) * 2.3283064365386963e-10;																\n"
"}																												\n"
"																												\n"
"vec2 Hammersley(uint i, uint N)																				\n"
"{																												\n"
"    return vec2(float(i) / float(N), RadicalInverse_VdC(i));													\n"
"}																												\n"
"																												\n"
"vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)														\n"
"{																												\n"
"    float a = roughness * roughness;																			\n"
"																												\n"
"    float phi = 2.0 * 3.14159265 * Xi.x;																		\n"
"    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));										\n"
"    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);															\n"
"																												\n"
"    vec3 H;																									\n"
"    H.x = cos(phi) * sinTheta;																					\n"
"    H.y = sin(phi) * sinTheta;																					\n"
"    H.z = cosTheta;																							\n"
"																												\n"
"    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);									\n"
"    vec3 tangent = normalize(cross(up, N));																	\n"
"    vec3 bitangent = cross(N, tangent);																		\n"
"    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;												\n"
"    return normalize(sampleVec);																				\n"
"}																												\n"
"																												\n"
"layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;												\n"
"void main()																									\n"
"{																												\n"
"    vec2 size = vec2(imageSize(_WriteTarget).xy);																\n"
"    ivec2 coord = ivec2(gl_GlobalInvocationID.xy + _Offset);													\n"
"    																											\n"
"    if (coord.x > size.x || coord.y > size.y)																	\n"
"    {																											\n"
"		return;																									\n"
"    }																											\n"
"    																											\n"
"    vec2 uv = (coord + (0.5f.xx / size)) / size;																\n"
"    vec3 N = OctaDecode(uv);																					\n"
"    vec3 R = N;																								\n"
"    vec3 V = R;																								\n"
"																												\n"
"    float totalWeight = 0.0;																					\n"
"    vec3 prefilteredColor = 0.0f.xxx;																			\n"
"																												\n"
"    for (uint i = 0u; i < SAMPLE_COUNT; ++i)																	\n"
"    {																											\n"
"        vec2 Xi = Hammersley(i, SAMPLE_COUNT);																	\n"
"        vec3 H = ImportanceSampleGGX(Xi, N, _Roughness);														\n"
"        vec3 L = normalize(2.0 * dot(V, H) * H - V);															\n"
"																												\n"
"        float NdotL = max(dot(N, L), 0.0);																		\n"
"																												\n"
"        if (NdotL > 0.0)																						\n"
"        {																										\n"
"            vec2 cylinderuv = CylinderUV(L);																	\n"
"            vec3 env = texture(_Source, cylinderuv).rgb;														\n"
"            prefilteredColor += env * NdotL;																	\n"
"            totalWeight += NdotL;																				\n"
"        }																										\n"
"    }																											\n"
"																												\n"
"    prefilteredColor /= totalWeight;																			\n"
"																												\n"
"    imageStore(_WriteTarget, coord, HDREncode(prefilteredColor));												\n"
"}																												\n";

const char* shader_vertex =
" #version 450 core																								\n"
" 																												\n"
" out vec2 vs_TEXCOORD0;																						\n"
" 																												\n"
" void main()																									\n"
" {																												\n"
"     vs_TEXCOORD0 = vec2( (gl_VertexID << 1) & 2, gl_VertexID & 2 );											\n"
"     gl_Position = vec4(vs_TEXCOORD0 * 2.0 - 1.0, 0.0, 1.0 );													\n"
" }																												\n";

const char* shader_fragment =
"	#version 450 core															\n"
"																				\n"
"	uniform sampler2D _Source;													\n"
"	uniform int _Lod;															\n"
"																				\n"
"	in vec2 vs_TEXCOORD0;														\n"
"	out vec4 SV_Target0;														\n"
"																				\n"
"	void main()																	\n"
"	{																			\n"
"		ivec2 coord = ivec2(vs_TEXCOORD0 * textureSize(_Source, _Lod).xy);		\n"
"		SV_Target0 = vec4(texelFetch(_Source, coord, _Lod).rgb, 1.0f);			\n"
"	}																			\n";

struct ShaderSource
{
	GLenum type;
	const char* source;
};

GLuint CompileShader(std::initializer_list<ShaderSource> sources)
{
	auto program = glCreateProgram();

	auto stageCount = sources.size();

	GLenum* glShaderIDs = reinterpret_cast<GLenum*>(alloca(sizeof(GLenum) * stageCount));

	int glShaderIDIndex = 0;

	for (auto& kv : sources)
	{
		auto type = kv.type;
		const auto& source = kv.source;

		auto glShader = glCreateShader(type);

		glShaderSource(glShader, 1, &source, 0);
		glCompileShader(glShader);

		GLint isCompiled = 0;
		glGetShaderiv(glShader, GL_COMPILE_STATUS, &isCompiled);

		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(glShader, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(glShader, maxLength, &maxLength, &infoLog[0]);

			glDeleteShader(glShader);

			printf(infoLog.data());
			glDeleteShader(glShader);
			continue;
		}

		glAttachShader(program, glShader);
		glShaderIDs[glShaderIDIndex++] = glShader;
	}

	glLinkProgram(program);

	GLint isLinked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);

	if (isLinked == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

		glDeleteProgram(program);

		for (auto i = 0; i < stageCount; ++i)
		{
			glDeleteShader(glShaderIDs[i]);
		}

		printf("Shader link failure! \n%s", infoLog.data());
		glDeleteProgram(program);
		return 0;
	}

	for (auto i = 0; i < stageCount; ++i)
	{
		glDetachShader(program, glShaderIDs[i]);
		glDeleteShader(glShaderIDs[i]);
	}
	
	return program;
}

GLuint LoadSourceKTX(const std::string& filepath)
{
	ktxTexture* kTexture;
	GLenum target, glerror;
	GLuint sourceTextureId;

	auto result = ktxTexture_CreateFromNamedFile(filepath.c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &kTexture);

	if (result != KTX_SUCCESS)
	{
		printf("Failed to load ktx! \n");
		return 0;
	}

	glGenTextures(1, &sourceTextureId);

	result = ktxTexture_GLUpload(kTexture, &sourceTextureId, &target, &glerror);

	if (result != KTX_SUCCESS)
	{
		glDeleteTextures(1, &sourceTextureId);
		ktxTexture_Destroy(kTexture);
		printf("Failed to upload ktx! \n");
		return 0;
	}

	glTextureParameteri(sourceTextureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTextureParameteri(sourceTextureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(sourceTextureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(sourceTextureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(sourceTextureId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	ktxTexture_Destroy(kTexture);

	return sourceTextureId;
}

std::vector<float> ParseRoughnessValues(const char* input)
{
	std::vector<float> output;

	auto separators = "{},";
	auto str = std::string(input);
	auto start = str.find_first_not_of(separators, 0);

	while (start != std::string::npos)
	{
		auto end = str.find_first_of(separators, start);

		if (end == std::string::npos)
		{
			output.push_back(std::stof(str.substr(start)));
			break;
		}

		output.push_back(std::stof(str.substr(start, end - start)));
		start = str.find_first_not_of(separators, end);
	}

	return output;
}

void ProcessFileExtension(std::string& filepath)
{
	if (filepath.empty())
	{
		return;
	}

	auto pos = filepath.find_last_of(".");

	if (pos == std::string::npos)
	{
		filepath += ".ktx";
		return;
	}

	filepath = filepath.substr(0, pos) + ".ktx";
}

uint32_t GetNextPowerOf2(uint32_t value)
{
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value++;

	return value;
}

int main(int argc, char* argv[])
{
	if (argc != 5)
	{
		printf("Invalid number of arguments!");
		return 0;
	}

	auto filepath = std::string(argv[1]);
	auto savepath = std::string(argv[2]);

	ProcessFileExtension(filepath);
	ProcessFileExtension(savepath);

	auto roughnessvalues = ParseRoughnessValues(argv[4]);
	auto mipCount = roughnessvalues.size();

	if (mipCount < 1)
	{
		printf("0 roughness levels specified!");
		return 0;
	}

	auto resolution = GetNextPowerOf2((uint32_t)std::stoi(argv[3]));
	auto maxMip = (uint32_t)log2(resolution);

	while (maxMip < mipCount)
	{
		resolution = GetNextPowerOf2(resolution + 1u);

		if (resolution > 2048)
		{
			printf("Output size limit exceeded!");
			return 0;
		}
	}

	GLFWwindow* window = nullptr;

	if (!glfwInit())
	{

		printf("Failed to initialize glfw!");
		return 0;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	window = glfwCreateWindow((int)512, (int)512, "Oem Exporter", nullptr, nullptr);

	if (!window)
	{
		printf("Failed to create a window!");
		glfwTerminate();
		return 0;
	}

	glfwMakeContextCurrent(window);

	int gladstatus = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	if (gladstatus == 0)
	{
		printf("Failed to load glad!");
		glfwDestroyWindow(window);
		glfwTerminate();
		return 0;
	}

	auto shaderCompute = CompileShader({ {GL_COMPUTE_SHADER, shader_compute} });
	auto shaderDisplay = CompileShader({ {GL_VERTEX_SHADER, shader_vertex}, {GL_FRAGMENT_SHADER, shader_fragment} });
	auto sourceTexure = LoadSourceKTX(filepath);

	if (shaderCompute == 0 || shaderDisplay == 0 || sourceTexure == 0)
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return 0;
	}

	GLuint targetTexture;
	glCreateTextures(GL_TEXTURE_2D, 1, &targetTexture);
	glTextureStorage2D(targetTexture, mipCount, GL_RGBA8, resolution, resolution);

	auto roughnessLocation = glGetUniformLocation(shaderCompute, "_Roughness");
	auto offsetLocation = glGetUniformLocation(shaderCompute, "_Offset");
	auto lodLocation = glGetUniformLocation(shaderDisplay, "_Lod");

	glUseProgram(shaderDisplay);
	glUniform1i(glGetUniformLocation(shaderDisplay, "_Source"), 1);

	glUseProgram(shaderCompute);
	glUniform1i(glGetUniformLocation(shaderCompute, "_WriteTarget"), 0);
	glUniform1i(glGetUniformLocation(shaderCompute, "_Source"), 0);

	glBindTextureUnit(0, sourceTexure);
	glBindTextureUnit(1, targetTexture);

	GLuint vertexArrayObject;
	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	printf("--------------------------------\n");

	uint32_t clustersTotal = 0u;
	uint32_t clusterIndex = 1u;

	for (auto i = 0; i < mipCount; ++i)
	{
		auto clusters = (uint32_t)ceil((resolution >> i) / 32.0f);
		clustersTotal += clusters * clusters;
	}

	for (auto i = 0; i < mipCount; ++i)
	{
		auto clusters = (uint32_t)ceil((resolution >> i) / 32.0f);

		glUniform1f(roughnessLocation, roughnessvalues.at(i));
		glBindImageTexture(0, targetTexture, i, true, 0, GL_WRITE_ONLY, GL_RGBA8);

		for (uint32_t x = 0; x < clusters; ++x)
		for (uint32_t y = 0; y < clusters; ++y)
		{
			printf("Generating level: %i, cluster: %i / %i", i, clusterIndex++, clustersTotal);
			printf(std::string(10, ' ').c_str());
			printf("\r");

			glUniform2ui(offsetLocation, x * 32u, y * 32u);
			glDispatchCompute(8, 8, 8);
			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

			glUseProgram(shaderDisplay);
			glUniform1i(lodLocation, i);
			glDrawArrays(GL_TRIANGLES, 0, 3);

			glUseProgram(shaderCompute);

			// This does slow down prosessing but allows for the console to update & avoids device timeout.
			glfwSwapBuffers(window);
		}
	}

	printf("\n");
	printf("--------------------------------\n");
	printf("Saving texture: %s \n", savepath.c_str());
	printf("--------------------------------\n");

	ktxTextureCreateInfo createInfo;
	createInfo.glInternalformat = GL_RGBA8;
	createInfo.baseWidth = resolution;
	createInfo.baseHeight = resolution;
	createInfo.baseDepth = 1;
	createInfo.numDimensions = 2;
	createInfo.numLevels = mipCount;
	createInfo.numLayers = 1;
	createInfo.numFaces = 1;
	createInfo.isArray = KTX_FALSE;
	createInfo.generateMipmaps = KTX_FALSE;

	ktxTexture1* texture = nullptr;
	ktxTexture1_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
	auto genericTex = ktxTexture(texture);

	for (auto i = 0; i < mipCount; ++i)
	{
		auto size = 4ull * (resolution >> i) * (resolution >> i);
		void* pixels = malloc(size);

		glGetTextureImage(targetTexture, i, GL_RGBA, GL_UNSIGNED_BYTE, size, pixels);
		ktxTexture_SetImageFromMemory(genericTex, i, 0, 0, reinterpret_cast<const ktx_uint8_t*>(pixels), size);
		
		free(pixels);
	}

	ktxTexture_WriteToNamedFile(genericTex, savepath.c_str());
	ktxTexture_Destroy(genericTex);

	printf("Texture generation complete! Size: %ipx, Mip count: %i \n", resolution, mipCount);

	glDeleteVertexArrays(1, &vertexArrayObject);
	glDeleteTextures(1, &targetTexture);
	glDeleteTextures(1, &sourceTexure);
	glDeleteProgram(shaderCompute);
	glfwDestroyWindow(window);
	glfwTerminate();
}
