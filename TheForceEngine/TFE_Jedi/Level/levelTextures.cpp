#include <climits>
#include <cstring>

#include "level.h"
#include "levelTextures.h"
#include "rwall.h"
#include "rtexture.h"
#include <TFE_Game/igame.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Asset/vocAsset.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>

#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/InfSystem/infTypesInternal.h>
#include <TFE_Jedi/InfSystem/message.h>

namespace TFE_Jedi
{
	bool level_getLevelTextures(TextureInfoList& textures)
	{
		s32 textureCount = 0;
		TextureData** levelTextures = level_getTextures(&textureCount);
		for (s32 i = 0; i < textureCount; i++)
		{
			TextureInfo texInfo = {};
			// Animated texture
			if (levelTextures[i]->uvWidth == BM_ANIMATED_TEXTURE)
			{
				texInfo.type = TEXINFO_DF_ANIM_TEX;
				texInfo.animTex = (AnimatedTexture*)levelTextures[i]->image;
			}
			else
			{
				texInfo.type = TEXINFO_DF_TEXTURE_DATA;
				texInfo.texData = levelTextures[i];
			}
			textures.push_back(texInfo);
		}

		// Insert animated textures.
		Allocator* animTextures = bitmap_getAnimatedTextures();
		AnimatedTexture* animTex = (AnimatedTexture*)allocator_getHead(animTextures);
		while (animTex)
		{
			TextureInfo texInfo = {};

			texInfo.type = TEXINFO_DF_ANIM_TEX;
			texInfo.animTex = animTex;
			textures.push_back(texInfo);
			animTex = (AnimatedTexture*)allocator_getNext(animTextures);
		}

		return !textures.empty();
	}

	bool level_getObjectTextures(TextureInfoList& textures)
	{
		// Insert sprite frames.
		std::vector<JediWax*> waxList;
		TFE_Sprite_Jedi::getWaxList(waxList);
		const size_t waxCount = waxList.size();
		JediWax** wax = waxList.data();
		for (size_t i = 0; i < waxCount; i++)
		{
			for (s32 animId = 0; animId < wax[i]->animCount; animId++)
			{
				WaxAnim* anim = WAX_AnimPtr(wax[i], animId);
				if (!anim) { continue; }
				for (s32 v = 0; v < 32; v++)
				{
					WaxView* view = WAX_ViewPtr(wax[i], anim, v);
					if (!view) { continue; }
					for (s32 f = 0; f < anim->frameCount; f++)
					{
						TextureInfo texInfo = {};
						texInfo.type = TEXINFO_DF_WAX_CELL;
						texInfo.frame = WAX_FramePtr(wax[i], view, f);
						texInfo.basePtr = wax[i];
						textures.push_back(texInfo);
					}
				}
			}
		}

		// Insert frames.
		std::vector<JediFrame*> frameList;
		TFE_Sprite_Jedi::getFrameList(frameList);
		const size_t frameCount = frameList.size();
		JediFrame** frame = frameList.data();
		for (size_t i = 0; i < frameCount; i++)
		{
			TextureInfo texInfo = {};
			texInfo.type = TEXINFO_DF_WAX_CELL;
			texInfo.frame = frame[i];
			texInfo.basePtr = frame[i];
			textures.push_back(texInfo);
		}

		// Insert 3DO textures.
		std::vector<JediModel*> modelList;
		TFE_Model_Jedi::getModelList(modelList);
		const size_t modelCount = modelList.size();
		JediModel** model = modelList.data();
		for (size_t i = 0; i < modelCount; i++)
		{
			for (s32 t = 0; t < model[i]->textureCount; t++)
			{
				TextureInfo texInfo = {};
				texInfo.type = TEXINFO_DF_TEXTURE_DATA;
				texInfo.texData = model[i]->textures[t];
				textures.push_back(texInfo);
			}
		}

		return !textures.empty();
	}
}
