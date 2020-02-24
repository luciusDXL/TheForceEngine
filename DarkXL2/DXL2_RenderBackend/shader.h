#pragma once
//////////////////////////////////////////////////////////////////////
// An OpenGL system to blit a texture to the screen. Uses a hardcoded
// basic shader + fullscreen triangle.
//
// Additional optional features can be added later such as filtering,
// bloom and GPU color conversion.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>

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

class Shader
{
public:
	Shader() : m_gpuHandle(0) {}

	bool create(const char* vertexShaderCode, const char* fragmentShaderCode);
	bool load(const char* vertexShader, const char* fragmentShader);
	void destroy();

	void bind();
	void unbind();

	void bindTextureNameToSlot(const char* texName, s32 slot);
	s32  getVariableId(const char* name);
	void setVariable(s32 id, ShaderVariableType type, const f32* data);

	inline u32 getHandle() { return m_gpuHandle; }

private:
	u32 m_gpuHandle;
};
