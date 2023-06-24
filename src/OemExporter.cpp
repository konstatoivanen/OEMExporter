#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <GLAD/glad.h>
#include <KTX/ktx.h>
#include "Shaders.h"

struct FormatParams
{
	size_t texelSize;
	GLenum internalFormat;
	GLenum diskFormat;
	GLenum channels;
	GLenum channelType;
	const char* computeSrc;
	const char* fragmentSrc;
};

constexpr static int32_t FORMAT_RGBM = 0;
constexpr static int32_t FORMAT_RGB16F = 1;
constexpr static int32_t FORMAT_RGB9E5 = 2;

constexpr static FormatParams FORMAT_PARAMETERS[3] =
{
	{ 4ull, GL_RGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, shader_compute_rgbm, shader_fragment },
	{ 8ull, GL_RGBA16F, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, shader_compute_rgb16f, shader_fragment },
	{ 4ull, GL_R32UI, GL_RGB9_E5, GL_RED_INTEGER, GL_UNSIGNED_INT, shader_compute_rgb9e5, shader_fragment_rgb9e5 },
};

struct ShaderSource
{
	GLenum type;
	const char* source;
};

static GLuint CompileShader(std::initializer_list<ShaderSource> sources)
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

static GLuint LoadSourceKTX(const std::string& filepath)
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

static std::vector<float> ParseRoughnessValues(const char* input)
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

static void ProcessFileExtension(std::string& filepath)
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

static uint32_t GetNextPowerOf2(uint32_t value)
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

static int GetExportFormatIndex(const char* input)
{

	if (strcmp(input, "RGBM") == 0)
	{
		return FORMAT_RGBM;
	}

	if (strcmp(input, "RGB16F") == 0)
	{
		return FORMAT_RGB16F;
	}

	if (strcmp(input, "RGB9E5") == 0)
	{
		return FORMAT_RGB9E5;
	}

	return -1;
}

int main(int argc, char* argv[])
{
	if (argc != 6)
	{
		printf("Invalid number of arguments!");
		return 0;
	}

	auto filepath = std::string(argv[1]);
	auto savepath = std::string(argv[2]);

	ProcessFileExtension(filepath);
	ProcessFileExtension(savepath);

	auto roughnessvalues = ParseRoughnessValues(argv[5]);
	auto mipCount = roughnessvalues.size();

	if (mipCount < 1)
	{
		printf("0 roughness levels specified!");
		return 0;
	}

	auto formatIndex = GetExportFormatIndex(argv[3]);
	
	if (formatIndex == -1)
	{
		printf("Invalid export format");
	}

	auto params = FORMAT_PARAMETERS[formatIndex];

	auto resolution = GetNextPowerOf2((uint32_t)std::stoi(argv[4]));
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

	auto shaderCompute = CompileShader({ {GL_COMPUTE_SHADER, params.computeSrc } });
	auto shaderDisplay = CompileShader({ {GL_VERTEX_SHADER, shader_vertex}, {GL_FRAGMENT_SHADER, params.fragmentSrc } });
	auto sourceTexure = LoadSourceKTX(filepath);

	if (shaderCompute == 0 || shaderDisplay == 0 || sourceTexure == 0)
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return 0;
	}

	GLuint targetTexture;
	glCreateTextures(GL_TEXTURE_2D, 1, &targetTexture);
	glTextureStorage2D(targetTexture, mipCount, params.internalFormat, resolution, resolution);

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
		glBindImageTexture(0, targetTexture, i, true, 0, GL_WRITE_ONLY, params.internalFormat);

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
	createInfo.glInternalformat = params.diskFormat;
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
		auto size = params.texelSize * (resolution >> i) * (resolution >> i);
		void* pixels = malloc(size);

		glGetTextureImage(targetTexture, i, params.channels, params.channelType, size, pixels);
		ktxTexture_SetImageFromMemory(genericTex, i, 0, 0, reinterpret_cast<const ktx_uint8_t*>(pixels), size);
		
		free(pixels);
	}

	ktxTexture_WriteToNamedFile(genericTex, savepath.c_str());
	ktxTexture_Destroy(genericTex);

	printf("Texture generation complete! Size: %ipx, Mip count: %i \n", (int32_t)resolution, (int32_t)mipCount);

	glDeleteVertexArrays(1, &vertexArrayObject);
	glDeleteTextures(1, &targetTexture);
	glDeleteTextures(1, &sourceTexure);
	glDeleteProgram(shaderCompute);
	glfwDestroyWindow(window);
	glfwTerminate();
}
