#include "openGL_Caps.h"
#include <GL/glew.h>
#include <assert.h>

enum CapabilityFlags
{
	CAP_PBO = (1 << 0),
	CAP_VBO = (1 << 1),
	CAP_FBO = (1 << 2),
	CAP_UBO = (1 << 3),
	CAP_NON_POW_2 = (1 << 4),
	CAP_TEXTURE_ARRAY = (1 << 5),

	CAP_2_1_FULL = (CAP_VBO | CAP_PBO | CAP_NON_POW_2),
	CAP_3_3_FULL = (CAP_PBO | CAP_VBO | CAP_FBO | CAP_UBO | CAP_NON_POW_2 | CAP_TEXTURE_ARRAY)
};

namespace OpenGL_Caps
{
	static u32 m_supportFlags = 0;
	static u32 m_deviceTier = 0;

	void queryCapabilities()
	{
		m_supportFlags = 0;
		if (GLEW_ARB_pixel_buffer_object)  { m_supportFlags |= CAP_PBO; }
		if (GLEW_ARB_vertex_buffer_object) { m_supportFlags |= CAP_VBO; }
		if (GLEW_ARB_framebuffer_object)   { m_supportFlags |= CAP_FBO; }
		if (GLEW_ARB_uniform_buffer_object){ m_supportFlags |= CAP_UBO; }
		if (GLEW_ARB_pixel_buffer_object)  { m_supportFlags |= CAP_NON_POW_2; }
		if (GLEW_EXT_texture_array)        { m_supportFlags |= CAP_TEXTURE_ARRAY; }

		if (GLEW_VERSION_4_5)
		{
			m_deviceTier = DEV_TIER_3;
		}
		else if (GLEW_VERSION_3_3 && (m_supportFlags&CAP_3_3_FULL) == CAP_3_3_FULL)
		{
			m_deviceTier = DEV_TIER_2;
		}
		else if (GLEW_VERSION_2_1 && (m_supportFlags&CAP_2_1_FULL) == CAP_2_1_FULL)
		{
			m_deviceTier = DEV_TIER_1;
		}
		else
		{
			m_deviceTier = DEV_TIER_0;
		}
	}

	bool supportsPbo()
	{
		return (m_supportFlags&CAP_PBO) != 0;
	}
	
	bool supportsVbo()
	{
		return (m_supportFlags&CAP_VBO) != 0;
	}

	bool supportsFbo()
	{
		return (m_supportFlags&CAP_FBO) != 0;
	}

	bool supportsNonPow2Textures()
	{
		return (m_supportFlags&CAP_NON_POW_2) != 0;
	}

	bool supportsTextureArrays()
	{
		return (m_supportFlags&CAP_TEXTURE_ARRAY) != 0;
	}

	bool deviceSupportsGpuBlit()
	{
		return m_deviceTier > DEV_TIER_0;
	}

	bool deviceSupportsGpuColorConversion()
	{
		return m_deviceTier > DEV_TIER_1;
	}

	bool deviceSupportsGpuRenderer()
	{
		return m_deviceTier > DEV_TIER_1;
	}

	u32 getDeviceTier()
	{
		return m_deviceTier;
	}
}