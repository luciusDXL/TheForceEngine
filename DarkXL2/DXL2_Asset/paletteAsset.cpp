#include "paletteAsset.h"
#include <DXL2_System/system.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Archive/archive.h>
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>

namespace DXL2_Palette
{
	typedef std::map<std::string, Palette256*> Palette256Map;
	static Palette256Map s_pal256;
	static std::vector<u8> s_buffer;
	static const char* c_defaultGob = "DARK.GOB";
	static Palette256 s_default;

#define CONV_6bitTo8bit(x) (((x)<<2) | ((x)>>4))

	void createDefault256()
	{
		for (u32 i = 0; i < 256; i++)
		{
			s_default.colors[i] = 0xff000000 | (i) | (i << 8) | (i << 16);
		}
	}

	Palette256* get256(const char* name)
	{
		Palette256Map::iterator iPal = s_pal256.find(name);
		if (iPal != s_pal256.end())
		{
			return iPal->second;
		}

		// It doesn't exist yet, try to load the palette.
		if (!DXL2_AssetSystem::readAssetFromGob(c_defaultGob, name, s_buffer))
		{
			return nullptr;
		}

		// Then convert from 24 bit color to 32 bit color.
		Palette256* pal = new Palette256;
		u8* src = s_buffer.data();
		for (u32 i = 0; i < 256; i++, src+=3)
		{
			pal->colors[i] = CONV_6bitTo8bit(src[0]) | (CONV_6bitTo8bit(src[1]) << 8) | (CONV_6bitTo8bit(src[2]) << 16) | (0xff << 24);
		}

		s_pal256[name] = pal;
		return pal;
	}

	Palette256* getDefault256()
	{
		return &s_default;
	}

	void freeAll()
	{
		Palette256Map::iterator iPal = s_pal256.begin();
		for (; iPal != s_pal256.end(); ++iPal)
		{
			Palette256* pal = iPal->second;
			delete pal;
		}
		s_pal256.clear();
	}
}
