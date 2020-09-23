#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Software Rendering Library
// The software rendering library handles rendering using the CPU,
// where the GPU is only used for blitting the results (and maybe
// post processing).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Renderer/renderer.h>
#include <TFE_Asset/paletteAsset.h>

struct ColorMap;
struct Texture;
struct TextureFrame;
struct Font;
struct Frame;
struct Sprite;
struct Model;

class TFE_SoftRenderCPU : public TFE_Renderer
{
public:
	TFE_SoftRenderCPU();

	bool init() override;
	void destroy() override;
	   
	void begin() override;
	void applyColorEffect() override;
	void end() override;

	bool changeResolution(u32 width, u32 height, bool asyncVirtualDisplay) override;
	void getResolution(u32* width, u32* height) override;
	void enableScreenClear(bool enable) override;

	// Temporary.
	u8*  getDisplay() override { return m_display; }

	// Draw Commands
	void setPalette(const Palette256* pal) override;
	void setColorMap(const ColorMap* cmp) override;
	void setPaletteColor(u8 index, u32 color) override;
	Palette256 getPalette() override;
	const ColorMap* getColorMap() override;
	void enablePalEffects(bool grayScale, bool green) override;
	void drawMapLines(s32 layers, f32 xOffset, f32 zOffset, f32 scale) override;
	void drawPalette() override;
	void drawTexture(Texture* texture, s32 x, s32 y) override;
	void drawTextureHorizontal(Texture* texture, s32 x, s32 y) override;
	void drawFont(Font* font) override;
	void drawFrame(Frame* frame, s32 x, s32 y) override;
	void drawSprite(Sprite* sprite, s32 x, s32 y, u32 anim, u8 angle) override;

	void blitImage(const TextureFrame* texture, s32 x0, s32 y0, s32 scaleX, s32 scaleY, u8 lightLevel = 31, TextureLayout layout = TEX_LAYOUT_VERT) override;
	void print(const char* text, const Font* font, s32 x0, s32 y0, s32 scaleX, s32 scaleY, u8 overrideColor = 0) override;
	s32  getStringPixelLength(const char* str, const Font* font) override;

	void drawLine(const Vec2f* v0, const Vec2f* v1, u8 color) override;
	void drawColoredColumn(s32 x0, s32 y0, s32 y1, u8 color) override;
	void drawTexturedColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight) override;
	void drawMaskTexturedColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight) override;
	void drawClampedTexturedColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight, s32 offset, bool skipTexels) override;
	void drawSkyColumn(s32 x0, s32 y0, s32 y1, s32 halfScrHeight, s32 offsetY, const u8* image, s32 texHeight) override;
	void drawSpriteColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight) override;
	void drawSpan(s32 x0, s32 x1, s32 y, f32 z, f32 r0, f32 r1, f32 xOffset, f32 zOffset, s8 lightLevel, s32 texWidth, s32 texHeight, const u8* image) override;
	void drawSpanClip(s32 x0, s32 x1, s32 y, f32 z, f32 r0, f32 r1, f32 xOffset, f32 zOffset, s8 lightLevel, s32 texWidth, s32 texHeight, const u8* image) override;

	void drawColoredQuad(s32 x, s32 y, s32 w, s32 h, u8 color) override;
	void drawHLine(s32 x0, s32 x1, s32 y, u8 color, u8 lightLevel) override;
	void drawHLineGouraud(s32 x0, s32 x1, s32 l0, s32 l1, s32 y, u8 color) override;
	void drawHLineTextured(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 texWidth, s32 texHeight, const u8* image, u8 lightLevel) override;
	void drawHLineTexturedPC(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 z0, s32 z1, s32 texWidth, s32 texHeight, const u8* image, u8 lightLevel) override;
	void drawHLineTexturedGouraud(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 l0, s32 l1, s32 texWidth, s32 texHeight, const u8* image) override;
	void drawHLineTexturedGouraudPC(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 z0, s32 z1, s32 l0, s32 l1, s32 texWidth, s32 texHeight, const u8* image) override;

	void setHLineClip(s32 x0, s32 x1, const s16* upperYMask, const s16* lowerYMask) override;

	// Basic pure horizontal and vertical lines
	void drawHorizontalLine(s32 x0, s32 x1, s32 y, u8 color) override;
	void drawVerticalLine(s32 y0, s32 y1, s32 x, u8 color) override;

	// Debug
	void clearMapMarkers() override;
	void addMapMarker(s32 layer, f32 x, f32 z, u8 color) override;
	u32 getMapMarkerCount() override;

private:
	u32 m_width;
	u32 m_height;
	bool m_asyncVirtualDisplay;
	f32 m_rcpHalfWidth;
	u8* m_display;
	u32* m_display32;
	Palette256 m_curPal;
	const ColorMap* m_curColorMap;

	bool m_enableGrayscale;
	bool m_enableNightVision;
	u8 m_grayScaleColorMap[256];
	u32 m_greenScalePal[256];

	u32 m_frame;
	bool m_clearScreen;

	void drawLineBresenham(f32 ax, f32 ay, f32 bx, f32 by, u8 color);
	bool clip(s32& x0, s32& y0, s32& w, s32& h);

	void buildGrayScaleColorMap(u32 start, u32 len, bool invert, const u32* pal);
	void buildGreenScalePalette(const u32* pal);
	void blitFontGlyph(const TextureFrame* texture, s32 x0, s32 y0, s32 scaleX, s32 scaleY, u8 overrideColor);
	u8 convertToGrayScale(u32 input);
};
