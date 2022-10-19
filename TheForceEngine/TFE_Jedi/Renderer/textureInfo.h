#pragma once
//////////////////////////////////////////////////////////////////////
// Jedi Renderer
// Classic Dark Forces (DOS) Jedi derived renderer.
//
// Copyright note:
// While the project as a whole is licensed under GPL-2.0, some of the
// code under TFE_JediRenderer/ was derived from reverse-engineered
// code from "Dark Forces" (DOS) which is copyrighted by LucasArts.
// The original renderer is contained in RClassic_Fixed/ and the
// RClassic_Float/ and RClassic_GPU/ sub-renderers are derived from
// that code.
//
// I consider the reverse-engineering to be "Fair Use" - a means of 
// supporting the games on other platforms and to improve support on
// existing platforms without claiming ownership of the games
// themselves or their IPs.
//
// That said using this code in a commercial project is risky without
// permission of the original copyright holders (LucasArts).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>

namespace TFE_Jedi
{
	// GPU renderer
	enum TextureInfoType
	{
		TEXINFO_DF_TEXTURE_DATA = 0,
		TEXINFO_DF_ANIM_TEX,
		TEXINFO_DF_WAX_CELL,
		TEXINFO_DF_DELT_TEX,
		TEXINFO_COUNT
	};

	struct TextureInfo
	{
		TextureInfoType type;
		union
		{
			TextureData* texData;
			AnimatedTexture* animTex;
			WaxFrame* frame;
		};
		void* basePtr = nullptr;	// used for WAX/Frame.
		s32 sortKey = 0;			// Calculated by Texture Packer.
	};
	typedef std::vector<TextureInfo> TextureInfoList;
	typedef bool(*TextureListCallback)(TextureInfoList& texList, AssetPool pool);
}