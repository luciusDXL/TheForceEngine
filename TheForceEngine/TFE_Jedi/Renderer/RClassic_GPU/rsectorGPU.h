#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include "../rsectorRender.h"
#include "../textureInfo.h"

class TextureGpu;

namespace TFE_Jedi
{
	class TFE_Sectors_GPU : public TFE_Sectors
	{
	public:
		TFE_Sectors_GPU() : m_gpuInit(false), m_levelInit(false), m_gpuBuffersAllocated(false), m_prevSectorCount(0), m_prevWallCount(0) {}

		// Sub-Renderer specific
		void destroy() override;
		void reset() override;
		void prepare() override;
		void draw(RSector* sector) override;
		void subrendererChanged() override;

		void flushCache();

		static TextureGpu* getColormap();

	private: 
		static bool updateBasePassShader();
	private:
		bool m_gpuInit;
		bool m_levelInit;
		bool m_gpuBuffersAllocated;
		s32  m_prevSectorCount;
		s32  m_prevWallCount;

	public:
	};
}  // TFE_Jedi
