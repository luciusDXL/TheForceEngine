#pragma once
//////////////////////////////////////////////////////////////////////
// TFE Post Process System
// Post Process effects are appended and executing in order.
// Effects can have an input image and output render target and can
// be chained together.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/dynamicTexture.h>

class PostProcessEffect;
struct PostEffectInput;

struct OverlayImage
{
	TextureGpu* texture;
	// Area of the texture to draw as an overlay.
	u32 overlayWidth;
	u32 overlayHeight;
	u32 overlayX;
	u32 overlayY;
};

typedef u32 OverlayID;

namespace TFE_PostProcess
{
	bool init();
	void destroy();

	void clearEffectStack();
	void appendEffect(PostProcessEffect* effect, u32 inputCount, const PostEffectInput* inputs, RenderTargetHandle output = nullptr, s32 x = 0, s32 y = 0, s32 width = 0, s32 height = 0);
		
	// Overlays - images drawn on top after the post process stack has executed.
	OverlayID addOverlayTexture(OverlayImage* overlay, f32* tint, f32 screenX, f32 screenY, f32 scale = 1.0f);
	const OverlayImage* getOverlayImage(OverlayID id);
	void setOverlayImage(OverlayID id, const OverlayImage* image);

	void modifyOverlay(OverlayID id, f32* tint, f32 screenX, f32 screenY, f32 scale = 1.0f);
	void getOverlaySettings(OverlayID id, f32* tint, f32* screenX, f32* screenY, f32* scale);
	void enableOverlay(OverlayID id, bool enable);
	void removeOverlay(OverlayID id);
	void removeAllOverlays();

	// Execute Post effects and overlays.
	void execute();

	// Internal
	void drawRectangle();
}
