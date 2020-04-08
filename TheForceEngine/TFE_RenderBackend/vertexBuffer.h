#pragma once
//////////////////////////////////////////////////////////////////////
// Wrapper for OpenGL textures that expose the limited set of 
// required functionality.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

// Maps between shader attribute name and attribute ID.
enum AttributeId
{
	ATTR_POS = 0,
	ATTR_NRM,
	ATTR_UV,
	ATTR_UV1,
	ATTR_UV2,
	ATTR_UV3,
	ATTR_COLOR,
	ATTR_COUNT
};

enum AttributeType
{
	ATYPE_FLOAT = 0,
	ATYPE_UINT32,
	ATYPE_INT32,
	ATYPE_UINT16,
	ATYPE_INT16,
	ATYPE_UINT8,
	ATYPE_INT8,
	ATYPE_COUNT
};

struct AttributeMapping
{
	AttributeId id;
	AttributeType type;
	u32 channels;		// Number of channels (usually 1 - 4)
	u32 offset;			// Attribute offset in bytes, 0 = compute automatically.
	bool normalized;
};

class VertexBuffer
{
public:
	VertexBuffer() : m_stride(0), m_count(0), m_attrCount(0), m_dynamic(false), m_gpuHandle(0), m_attrMapping(nullptr) {}
	~VertexBuffer();

	bool create(u32 count, u32 stride, u32 attrCount, const AttributeMapping* attrMapping, bool dynamic, void* initData = nullptr);
	void destroy();

	void update(const void* buffer, size_t size);
	void bind();
	void unbind();

	inline u32 getHandle() const { return m_gpuHandle; }

private:
	u32 m_stride;
	u32 m_count;
	u32 m_size;
	u32 m_gpuHandle;
	u32 m_attrCount;
	bool m_dynamic;

	AttributeMapping* m_attrMapping;
};
