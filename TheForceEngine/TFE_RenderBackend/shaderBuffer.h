#pragma once
//////////////////////////////////////////////////////////////////////
// Wrapper for "shader buffers" - which can either be backed by 
// Buffer Textures or Shader Storage Buffers depending on capabilities.
// Typically hardware will be limited to strides as: 
//   * 1, 2, or 4 channels
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

enum BufferChannelType
{
	BUF_CHANNEL_UINT = 0,
	BUF_CHANNEL_INT,
	BUF_CHANNEL_FLOAT,
	BUF_CHANNEL_COUNT,
	BUF_CHANNEL_INVALID,
};

struct ShaderBufferDef
{
	u32 channelCount;	// 1, 2, 4 channels (R, RG, RGBA)
	u32 channelSize;	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
	BufferChannelType channelType;
};

class ShaderBuffer
{
public:
	ShaderBuffer() : m_stride(0), m_count(0), m_size(0), m_initialized(false) {}
	~ShaderBuffer();

	bool create(u32 count, const ShaderBufferDef& bufferDef, bool dynamic, void* initData = nullptr);
	void destroy();

	void update(const void* buffer, size_t size);
	void bind(s32 bindPoint);
	void unbind(s32 bindPoint);

	inline u32 getHandle() const { return m_gpuHandle[0]; }

private:
	ShaderBufferDef m_bufferDef;
	u32 m_stride;
	u32 m_count;
	u32 m_size;
	u32 m_gpuHandle[2];
	bool m_dynamic;
	bool m_initialized;
};
