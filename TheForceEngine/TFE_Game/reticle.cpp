#include "reticle.h"
#include <TFE_PostProcess/postprocess.h>
#include <TFE_System/math.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <algorithm>
#include <cstring>

enum
{
	RETICLE_SIZE  = 32,
	RETICLE_COL   = 8,
	RETICLE_COUNT = 18,
};

static OverlayID s_reticleId = 0;
static bool s_enabled = false;
static u32 s_shapeIndex = 0;
static f32 s_scale = 1.0f;
static f32 s_color[4] = { 0 };

static OverlayImage s_reticleImage = {};
static bool s_reticleLoaded = false;

static TFE_Settings_Graphics* s_settings;

bool reticle_init()
{
	s_settings = TFE_Settings::getGraphicsSettings();

	char imagePath[TFE_MAX_PATH];
	sprintf(imagePath, "UI_Images/ReticleAtlas.png");
	if (!TFE_Paths::mapSystemPath(imagePath)) {
		TFE_Paths::appendPath(TFE_PathType::PATH_PROGRAM, "UI_Images/ReticleAtlas.png", imagePath, TFE_MAX_PATH);
	}

	Image* image = TFE_Image::get(imagePath);
	if (!image)
	{
		TFE_System::logWrite(LOG_ERROR, "Reticle", "Cannot load reticle atlas: '%s'.", imagePath);
		return false;
	}

	s_reticleImage.texture = TFE_RenderBackend::createTexture(image->width, image->height, image->data);
	s_reticleImage.overlayX = 0;
	s_reticleImage.overlayY = 0;
	s_reticleImage.overlayWidth = 32;
	s_reticleImage.overlayHeight = 32;
	s_reticleId = TFE_PostProcess::addOverlayTexture(&s_reticleImage, nullptr, 0.5f, 0.5f);
	TFE_PostProcess::enableOverlay(s_reticleId, false);

	s_enabled = false;
	s_reticleLoaded = true;

	// Values from settings.
	reticle_setShape((u32)s_settings->reticleIndex);
	reticle_setScale(s_settings->reticleScale);
	reticle_setColor(&s_settings->reticleRed);

	return true;
}

void reticle_destroy()
{
	TFE_PostProcess::removeOverlay(s_reticleId);
	s_reticleLoaded = false;
}

void reticle_enable(bool enable)
{
	if (!s_reticleLoaded) { return; }
	s_enabled = enable && s_settings->reticleEnable;
	TFE_PostProcess::enableOverlay(s_reticleId, s_enabled);
}

void reticle_setShape(u32 index)
{
	if (!s_reticleLoaded) { return; }
	s_shapeIndex = std::min(index, (u32)RETICLE_COUNT-1);

	s_reticleImage.overlayX = (index % RETICLE_COL) * RETICLE_SIZE;
	s_reticleImage.overlayY = (index / RETICLE_COL) * RETICLE_SIZE;
	TFE_PostProcess::setOverlayImage(s_reticleId, &s_reticleImage);
}

void reticle_setColor(const f32* color)
{
	if (!color || !s_reticleLoaded) { return; }
	for (s32 i = 0; i < 4; i++) { s_color[i] = color[i]; }
	TFE_PostProcess::modifyOverlay(s_reticleId, s_color, 0.5f, 0.5f, s_scale);
}

void reticle_setScale(f32 scale)
{
	if (!s_reticleLoaded) { return; }
	s_scale = scale;
	TFE_PostProcess::modifyOverlay(s_reticleId, s_color, 0.5f, 0.5f, s_scale);
}

bool reticle_enabled()
{
	return s_enabled && s_reticleLoaded;
}

u32 reticle_getShape()
{
	return s_shapeIndex;
}

u32 reticle_getShapeCount()
{
	return RETICLE_COUNT;
}

void reticle_getColor(f32* color)
{
	for (s32 i = 0; i < 4; i++) { color[i] = s_color[i]; }
}

f32 reticle_getScale()
{
	return s_scale;
}
