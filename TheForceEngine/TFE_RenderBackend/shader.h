#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <string>

enum ShaderVariableType
{
	SVT_SCALAR = 0,
	SVT_VEC2,
	SVT_VEC3,
	SVT_VEC4,
	SVT_MAT3x3,
	SVT_MAT4x3,
	SVT_MAT4x4,
	SVT_COUNT
};

enum ShaderVersion
{
	SHADER_VER_COMPTABILE = 0,	// Lowest support version.
	SHADER_VER_STD,				// Standard, minimum requirement for GPU renderer.
	SHADER_VER_COMPUTE,			// Shader version with compute support.
};

struct ShaderDefine
{
	std::string name;
	std::string value;
};

class Shader
{
public:
	Shader() : m_gpuHandle(0), m_shaderVersion(SHADER_VER_COMPTABILE) {}

	bool create(const char* vertexShaderCode, const char* fragmentShaderCode, const char* defineString = nullptr, ShaderVersion version = SHADER_VER_COMPTABILE);
	bool load(const char* vertexShader, const char* fragmentShader, u32 defineCount = 0, ShaderDefine* defines = nullptr, ShaderVersion version = SHADER_VER_COMPTABILE);
	void destroy();

	void bind();
	static void unbind();

	void bindTextureNameToSlot(const char* texName, s32 slot);
	s32  getVariableId(const char* name);
	void setVariable(s32 id, ShaderVariableType type, const f32* data);

	inline u32 getHandle() { return m_gpuHandle; }

private:
	u32 m_gpuHandle;
	ShaderVersion m_shaderVersion;
};
