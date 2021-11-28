#include <cstring>

#include "colormapAsset.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Archive/archive.h>
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>

namespace TFE_ColorMap
{
	typedef std::map<std::string, ColorMap*> ColorMapMap;
	static ColorMapMap s_colormaps;
	static std::vector<u8> s_buffer;
	static const char* c_defaultGob = "DARK.GOB";

	// Allocate a new color map.
	ColorMap* allocate(const char* name)
	{
		ColorMapMap::iterator iColorMap = s_colormaps.find(name);
		if (iColorMap != s_colormaps.end())
		{
			return iColorMap->second;
		}

		ColorMap* colormap = new ColorMap;
		s_colormaps[name] = colormap;
		return colormap;
	}

	ColorMap* get(const char* name)
	{
		ColorMapMap::iterator iColorMap = s_colormaps.find(name);
		if (iColorMap != s_colormaps.end())
		{
			return iColorMap->second;
		}

		// It doesn't exist yet, try to load the colormap.
		if (TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			// Create the font.
			ColorMap* colormap = new ColorMap;
			// We should be able to read the whole file in.
			if (s_buffer.size() >= sizeof(ColorMap))
			{
				memcpy(colormap, s_buffer.data(), sizeof(ColorMap));
			}

			s_colormaps[name] = colormap;
			return colormap;
		}

		return nullptr;
	}

	void freeAll()
	{
		ColorMapMap::iterator iColorMap = s_colormaps.begin();
		for (; iColorMap != s_colormaps.end(); ++iColorMap)
		{
			ColorMap* colormap = iColorMap->second;
			delete colormap;
		}
		s_colormaps.clear();
	}

	u8 findClosestColor(const u8* litColor, const u8* colors)
	{
		s32 minDiff = 256;
		s32 rL = litColor[0];
		s32 gL = litColor[1];
		s32 bL = litColor[2];

		u8 closest = 0;
		for (s32 i = 0; i < 256; i++)
		{
			s32 I = i << 2;
			s32 rI = colors[I + 0];
			s32 gI = colors[I + 1];
			s32 bI = colors[I + 2];

			s32 diff[] = { abs(rL - rI), abs(gL - gI), abs(bL - bI) };
			s32 maxDiff = std::max(diff[0], std::max(diff[1], diff[2]));

			if (maxDiff < minDiff)
			{
				minDiff = maxDiff;
				closest = i;
			}

			if (minDiff == 0) { break; }
		}

		return closest;
	}

	ColorMap* generateColorMap(const char* newFile, const Palette256* pal)
	{
		// Get an existing colormap to use as a "template" - changing only the required parts
		// to match the palette.
		const ColorMap* templateMap = TFE_ColorMap::get("TALAY.CMP");
		ColorMap* newColorMap = allocate(newFile);
		memcpy(newColorMap, templateMap, sizeof(ColorMap));

		// fixup after the first 32 colors.
		const u8* palColors = (u8*)pal->colors;
		for (s32 C = 32; C < 256; C++)
		{
			const u8* color = &palColors[C << 2];

			// Get the base color.
			for (s32 L = 0; L < 32; L++)
			{
				// Only changes indices that are now invalid.
				u8 curIndex = newColorMap->colorMap[L * 256 + C];
				//if (curIndex < 208) { continue; }

				u8 litColor[3];
				litColor[0] = u8(s32(color[0]) * L / 31);
				litColor[1] = u8(s32(color[1]) * L / 31);
				litColor[2] = u8(s32(color[2]) * L / 31);

				// Now find the closest color.
				if (L == 31)
				{
					newColorMap->colorMap[L * 256 + C] = C;
				}
				else
				{
					newColorMap->colorMap[L * 256 + C] = findClosestColor(litColor, palColors);
				}
			}
		}
		return newColorMap;
	}
}
