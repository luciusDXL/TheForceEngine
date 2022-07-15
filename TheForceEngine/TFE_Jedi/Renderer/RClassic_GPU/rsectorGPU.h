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
		TFE_Sectors_GPU() : m_gpuInit(false) {}

		// Sub-Renderer specific
		void reset() override;
		void prepare() override;
		void draw(RSector* sector) override;
		void subrendererChanged() override;

		static TextureGpu* getColormap();

	private: 
		bool updateBasePassShader();
	private:
		bool m_gpuInit;

	public:
	};
}  // TFE_Jedi
