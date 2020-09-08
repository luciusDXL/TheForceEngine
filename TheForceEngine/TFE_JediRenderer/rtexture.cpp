#include "rtexture.h"
#include <TFE_System/system.h>

namespace TFE_JediRenderer
{
	TextureFrame* texture_getFrame(Texture* texture, u32 baseFrame)
	{
		if (!texture) { return nullptr; }
		if (!texture->frameCount)
		{
			return &texture->frames[0];
		}
		else if (texture->frameCount && texture->frameRate == 0.0f)
		{
			return &texture->frames[baseFrame];
		}

		const f64 time = TFE_System::getTime();

		// Animated texture, so override the frame.
		const u32 frame = u32(f32(texture->frameRate) * time) % texture->frameCount;
		return &texture->frames[frame];
	}
}
