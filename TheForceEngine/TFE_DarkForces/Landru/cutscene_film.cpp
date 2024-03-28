#include "cutscene_film.h"
#include "lsystem.h"
#include "lactorDelt.h"
#include "lactorAnim.h"
#include "lactorCust.h"
#include "lcanvas.h"
#include "lfade.h"
#include "lsound.h"
#include "time.h"
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_System/endian.h>
#include <TFE_Archive/lfdArchive.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>
#include <cstring>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	struct FilmState
	{
		Film* firstFilm = nullptr;
		FadeType filmFade = ftype_noFade;
		FadeColorType filmColorFade = fcolorType_noColorFade;
	};
	static FilmState s_filmState = {};

	LActor* cutsceneFilm_cloneActor(u32 type, const char* name);
	LSound* cutsceneFilm_cloneSound(u32 type, const char* name);
	void cutsceneFilm_initFilm(Film* film, u8** array, LRect* frame, s16 x, s16 y, s16 zPlane);
	void cutsceneFilm_setName(Film* film, u32 resType, const char* name);
	JBool cutsceneFilm_isTimeStamp(Film* film, FilmObject* filmObj, u8* data);
	void cutsceneFilm_setActorToFilm(FilmObject* filmObj, u8* data);
	void cutsceneFilm_setViewToFilm(Film* film, FilmObject* filmObj, u8* data);
	void cutsceneFilm_setFade(s16 fade, s16 colorFade);
	void cutsceneFilm_stepView(Film* film, FilmObject* filmObj, u8* data);
	void cutsceneFilm_stepPalette(Film* film, FilmObject* filmObj, u8* data);
	void cutsceneFilm_stepSound(Film* film, FilmObject* filmObj, u8* data);
	void cutsceneFilm_rewind(Film* film);
	void cutsceneFilm_update(Film* film);
	void cutsceneFilm_freeData(Film* film);
	void cutsceneFilm_freeDataObject(Film* film, s16 index);

	void cutsceneFilm_setFade(s16 fade, s16 colorFade);
	void cutsceneFilm_clearFade();
	void cutsceneFilm_startFade(JBool paletteChange);
	JBool cutsceneFilm_isFading();

	void cutsceneFilm_reset()
	{
		s_filmState = {};
	}

	Film* cutsceneFilm_allocate()
	{
		Film* film = (Film*)landru_alloc(sizeof(Film));
		if (!film) { return nullptr; }

		memset(film, 0, sizeof(Film));
		film->stop = -1;
		film->flags = CF_STATE_REFRESH;

		return film;
	}

	void cutsceneFilm_free(Film* film)
	{
		if (!film) { return; }

		if (film->varPtr)
		{
			landru_free(film->varPtr);
		}
		if (film->varPtr2)
		{
			landru_free(film->varPtr2);
		}
		if (film->flags & CF_STATE_DISCARD)
		{
			cutsceneFilm_freeData(film);
		}
		landru_free(film);
	}

	void cutsceneFilm_freeData(Film* film)
	{
		if (!film->array) { return; }

		for (s32 i = 0; i < film->arraySize; i++)
		{
			cutsceneFilm_freeDataObject(film, i);
		}

		landru_free(film->array);
		film->array = nullptr;
	}

	void cutsceneFilm_freeDataObject(Film* film, s16 index)
	{
		FilmObject* filmObj = (FilmObject*)film->array[index];
		if (filmObj)
		{
			switch (filmObj->id)
			{
				case CF_FILE_ACTOR:
				{
					LActor* actor = (LActor*)filmObj->data;
					lactor_removeActor(actor);
					lactor_free(actor);
				} break;
				case CF_FILE_PALETTE:
				{
					LPalette* pal = (LPalette*)filmObj->data;
					lpalette_remove(pal);
					lpalette_free(pal);
				} break;
				case CF_FILE_SOUND:
				{
					LSound* sound = (LSound*)filmObj->data;
					stopSound(sound);
				} break;
			}
			landru_free(filmObj);
			film->array[index] = nullptr;
		}
	}

	JBool cutsceneFilm_loadResources(FileStream* file, u8** array, s16 arraySize)
	{
		for (s32 i = 0; i < arraySize; i++)
		{
			s32 type;
			file->read(&type);
			type = TFE_Endian::be32_to_cpu(type);

			char name[32];
			file->readBuffer(name, 8);
			name[8] = 0;

			u32 size;
			s16 id, chunks, used;
			file->read(&size);
			file->read(&id);
			file->read(&chunks);
			file->read(&used);
			size = TFE_Endian::le32_to_cpu(size);
			id = TFE_Endian::le16_to_cpu(id);
			chunks = TFE_Endian::le16_to_cpu(chunks);
			used = TFE_Endian::le16_to_cpu(used);

			// Allocate space for the object.
			FilmObject* obj = (FilmObject*)landru_alloc(sizeof(FilmObject) + used);
			if (!obj)
			{
				return JFALSE;
			}

			obj->resType = type;
			strcpy(obj->name, name);
			obj->id = id;
			obj->offset = 0;
			obj->data   = nullptr;

			u8* objData = (u8*)obj + sizeof(FilmObject);
			file->readBuffer(objData, used);
			
			array[i] = (u8*)obj;
		}
		return JTRUE;
	}

	LActor* cutsceneFilm_cloneActor(u32 type, const char* name)
	{
		LActor* curActor = lactor_getList();
		LActor* actor = nullptr;
		s32 equal = 0;
		while (curActor && !actor)
		{
			if (type == curActor->resType && strcmp(name, curActor->name) == 0)
			{
				actor = curActor;
			}
			curActor = curActor->next;
		}

		curActor = nullptr;
		if (actor)
		{
			LRect rect;
			lcanvas_getClip(&rect);

			if (actor->resType == CF_TYPE_DELTA_ACTOR)
			{
				curActor = lactorDelt_alloc(nullptr, &rect, 0, 0, 0);
			}
			else
			{
				curActor = lactorAnim_alloc(nullptr, &rect, 0, 0, 0);
			}

			if (curActor)
			{
				lactor_copyData(curActor, actor);
				lactor_setName(curActor, type, name);
			}
		}
		return curActor;
	}

	LSound* cutsceneFilm_cloneSound(u32 type, const char* name)
	{
		LSound* curSound = lSoundGetList();
		LSound* sound = nullptr;

		while (curSound && !sound)
		{
			s32 equal = 0;
			if (type == curSound->resType)
			{
				equal = 1;

				s32 i = 0;
				for (; i < 8 && name[i]; i++)
				{
					if (name[i] != curSound->name[i]) { equal = 0; }
				}
				if (i < 8 && curSound->name[i] && equal)
				{
					equal = 0;
				}
			}
			if (equal)
			{
				sound = curSound;
			}

			curSound = curSound->next;
		}

		LSound* clonedSound = nullptr;
		if (sound)
		{
			clonedSound = lSoundAlloc(nullptr);
			if (clonedSound)
			{
				copySoundData(clonedSound, sound);
				setSoundName(clonedSound, type, name);
			}
		}
		return clonedSound;
	}

	JBool cutsceneFilm_readObject(u32 type, const char* name, u8** obj)
	{
		LRect rect;
		lcanvas_getBounds(&rect);

		LActor* actor = nullptr;
		LPalette* pal = nullptr;
		LSound* sound = nullptr;
		JBool retValue = JTRUE;
		if (type != CF_TYPE_VIEW && type != CF_TYPE_CUSTOM_ACTOR)
		{
			if (type == CF_TYPE_DELTA_ACTOR)
			{
				actor = cutsceneFilm_cloneActor(type, name);
				if (!actor)
				{
					actor = lactorDelt_load(name, &rect, 0, 0, 0);
				}
				if (!actor) { retValue = JFALSE; }
			}
			else if (type == CF_TYPE_ANIM_ACTOR)
			{
				actor = cutsceneFilm_cloneActor(type, name);
				if (!actor)
				{
					actor = lactorAnim_load(name, &rect, 0, 0, 0);
				}
				if (!actor) { retValue = JFALSE; }
			}
			else if (type == CF_TYPE_PALETTE)
			{
				pal = lpalette_load(name);
				if (!pal) { retValue = JFALSE; }
			}
			else if (type == CF_TYPE_GMIDI)
			{
				sound = nullptr;
				assert(0);
				if (!sound) { retValue = JFALSE; }
			}
			else if (type == CF_TYPE_VOC_SOUND)
			{
				sound = cutsceneFilm_cloneSound(digitalSound, name);
				if (!sound)
				{
					sound = lSoundLoad(name, digitalSound);
				}
				if (!sound) { retValue = JFALSE; }
			}
		}
		else if (type == CF_TYPE_CUSTOM_ACTOR)
		{
			actor = lactorCust_alloc(nullptr, &rect, 0, 0, 0);
			if (actor)
			{
				lactor_setDrawFunc(actor, nullptr);
			}
			if (!actor) { retValue = JFALSE; }
		}

		if (actor)
		{
			lactor_setTime(actor, -1, -1);
			*obj = (u8*)actor;
		}
		else if (pal)
		{
			*obj = (u8*)pal;
		}
		else
		{
			*obj = (u8*)sound;
		}
		return retValue;
	}

	JBool cutsceneFilm_readObjects(Film* film, FilmLoadCallback filmCallback)
	{
		for (s32 i = 0; i < film->arraySize; i++)
		{
			FilmObject* obj = (FilmObject*)film->array[i];

			// Read the object.
			if (cutsceneFilm_readObject(obj->resType, obj->name, &obj->data))
			{
				if (filmCallback && filmCallback(film, obj))
				{
					landru_free(obj->data);
					landru_free(obj);
					film->array[i] = nullptr;
				}
			}
			else
			{
				return JFALSE;
			}
		}

		return JTRUE;
	}

	Film* cutsceneFilm_load(const char* name, LRect* frameRect, s16 x, s16 y, s16 z, FilmLoadCallback callback)
	{
		Film* film = cutsceneFilm_allocate();
		if (!film) { return nullptr; }

		u8** array = nullptr;
		FilePath resPath;
		char filmName[32];
		sprintf(filmName, "%s.FILM", name);

		if (TFE_Paths::getFilePath(filmName, &resPath))
		{
			FileStream file;
			file.open(&resPath, Stream::MODE_READ);

			s16 version;
			s16 cellCount;
			s16 arraySize;
			file.read(&version);
			file.read(&cellCount);
			file.read(&arraySize);
			version = TFE_Endian::le16_to_cpu(version);
			cellCount = TFE_Endian::le16_to_cpu(cellCount);
			arraySize = TFE_Endian::le16_to_cpu(arraySize);

			if (version != CF_VERSION)
			{
				file.close();
				landru_free(film);
				return nullptr;
			}

			array = (u8**)landru_alloc(sizeof(u8*) * arraySize);
			memset(array, 0, sizeof(u8*) * arraySize);
			if (array)
			{
				film->array = array;
				film->arraySize = arraySize;
				film->curCell = 0;
				film->cellCount = cellCount;

				if (!cutsceneFilm_loadResources(&file, film->array, film->arraySize))
				{
					landru_free(film);
					landru_free(array);
					film = nullptr;
				}
			}
			file.close();

			if (!cutsceneFilm_readObjects(film, callback))
			{
				landru_free(film);
				landru_free(array);
				film = nullptr;
				array = nullptr;
			}
		}

		if (array)
		{
			cutsceneFilm_initFilm(film, array, frameRect, x, y, z);
			cutsceneFilm_setName(film, CF_TYPE_FILM, name);
		}
		else
		{
			landru_free(film);
			film = nullptr;
		}

		return film;
	}

	void cutsceneFilm_setName(Film* film, u32 resType, const char* name)
	{
		film->resType = resType;
		strcpy(film->name, name);
	}
		
	void cutsceneFilm_initFilm(Film* film, u8** array, LRect* frame, s16 x, s16 y, s16 zPlane)
	{
		film->frameRect = *frame;
		film->x = x;
		film->y = y;
		film->zplane = zPlane;
		cutsceneFilm_discardData(film);

		film->drawFunc = nullptr;
		film->updateFunc = cutsceneFilm_update;
		film->array = array;

		cutsceneFilm_add(film);
	}
		
	void cutsceneFilm_discardData(Film* film)
	{
		film->flags |= CF_STATE_DISCARD;
	}

	void cutsceneFilm_keepData(Film* film)
	{
		film->flags &= ~CF_STATE_DISCARD;
	}

	JBool cutsceneFilm_isTimeStamp(Film* film, FilmObject* filmObj, u8* data)
	{
		s16* chunk = (s16*)(data + filmObj->offset);
		if (TFE_Endian::le16_to_cpu(chunk[1]) == CF_CMD_TIMESTAMP && TFE_Endian::le16_to_cpu(chunk[2]) == (s16)film->curCell)
		{
			return JTRUE;
		}
		return JFALSE;
	}

	void cutsceneFilm_rewindView(Film* film, FilmObject* filmObj, u8* data)
	{
		filmObj->offset = 0;
		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepView(film, filmObj, data);
		}
	}

	void cutsceneFilm_rewindPalette(Film* film, FilmObject* filmObj, u8* data)
	{
		filmObj->offset = 0;
		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepPalette(film, filmObj, data);
		}
	}

	void cutsceneFilm_rewindActor(Film* film, FilmObject* filmObj, u8* data)
	{
		LActor* actor = (LActor*)filmObj->data;
		lcanvas_getClip(&actor->frame);

		lactor_setPos(actor, 0, 0, 0, 0);
		lactor_setSpeed(actor, 0, 0, 0, 0);
		lactor_setState(actor, 0, 0);
		lactor_setZPlane(actor, 0);
		lactor_setFlip(actor, JFALSE, JFALSE);

		if (lactor_isVisible(actor))
		{
			lactor_hide(actor);
		}

		actor->var1 = 0;
		actor->var2 = 0;
		filmObj->offset = 0;

		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepActor(film, filmObj, data);
		}
	}

	void cutsceneFilm_rewindSound(Film* film, FilmObject* filmObj, u8* data)
	{
		LSound* sound = (LSound*)filmObj->data;
		filmObj->offset = 0;

		stopSound(sound);
		sound->var1 = 0;
		sound->var2 = 0;

		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepSound(film, filmObj, data);
		}
	}

	void cutsceneFilm_rewindObjects(Film* film)
	{
		film->curCell = 0;

		// Store the screen palette.
		lpalette_copyScreenToDest(0, 0, 255);
		lpalette_copyScreenToSrc(0, 0, 255);
		// Clear the palette
		lpalette_setDstColor(0, 255, 0, 0, 0);

		if (film->def_palette)
		{
			lpalette_setDstPal(film->def_palette);
		}

		for (s32 i = 0; i < film->arraySize; i++)
		{
			FilmObject* filmObj = (FilmObject*)film->array[i];
			if (filmObj)
			{
				switch (filmObj->id)
				{
					case CF_FILE_VIEW:
					{
						cutsceneFilm_rewindView(film, filmObj, (u8*)(filmObj + 1));
					} break;
					case CF_FILE_PALETTE:
					{
						cutsceneFilm_rewindPalette(film, filmObj, (u8*)(filmObj + 1));
					} break;
					case CF_FILE_ACTOR:
					{
						cutsceneFilm_rewindActor(film, filmObj, (u8*)(filmObj + 1));
					} break;
					case CF_FILE_SOUND:
					{
						cutsceneFilm_rewindSound(film, filmObj, (u8*)(filmObj + 1));
					} break;
				}
			}
		}

		if (!lpalette_compareSrcDst())
		{
			if (cutsceneFilm_isFading())
			{
				cutsceneFilm_startFade(JTRUE);
			}
			else if (!lfade_isActive())
			{
				lfade_startColorFade(fcolorType_snapColorFade, JTRUE, 1, 0, 1);
			}
			else
			{
				lpalette_copyDstToScreen(0, 0, 255);
				lpalette_putScreenPal();
			}
		}
		else if (cutsceneFilm_isFading())
		{
			cutsceneFilm_startFade(JFALSE);
		}

		// Advance to the next frame.
		film->curCell++;
	}

	void cutsceneFilm_stepFilmFrame(FilmObject* filmObj, u8* data)
	{
		s16* chunk = (s16*)(data + filmObj->offset);
		filmObj->offset += TFE_Endian::le16_to_cpu(chunk[0]);
	}

	JBool cutsceneFilm_isNextFrame(FilmObject* filmObj, u8* data)
	{
		s16* chunk = (s16*)(data + filmObj->offset);
		if (TFE_Endian::le16_to_cpu(chunk[1]) == CF_CMD_TIMESTAMP || TFE_Endian::le16_to_cpu(chunk[1]) == CF_CMD_END)
		{
			return JTRUE;
		}
		return JFALSE;
	}

	void cutsceneFilm_setActorToFilm(FilmObject* filmObj, u8* data)
	{
		s16* chunk = (s16*)(data + filmObj->offset);
		u32 type = TFE_Endian::le16_to_cpu(chunk[1]);
		LActor* actor = (LActor*)filmObj->data;
		chunk += 2;

		switch (type)
		{
			case CF_CMD_ACTOR_POS:
				lactor_setPos(actor, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]), TFE_Endian::le16_to_cpu(chunk[2]), TFE_Endian::le16_to_cpu(chunk[3]));
				break;

			case CF_CMD_ACTOR_VEL:
				lactor_setSpeed(actor, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]), TFE_Endian::le16_to_cpu(chunk[2]), TFE_Endian::le16_to_cpu(chunk[3]));
				break;

			case CF_CMD_ACTOR_Z:
				lactor_setZPlane(actor, TFE_Endian::le16_to_cpu(chunk[0]));
				break;

			case CF_CMD_ACTOR_STATE:
				lactor_setState(actor, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]));
				break;

			case CF_CMD_ACTOR_STATEV:
				lactor_setStateSpeed(actor, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]));
				break;

			case CF_CMD_ACTOR_VAR1:
				actor->var1 = TFE_Endian::le16_to_cpu(chunk[0]);
				break;

			case CF_CMD_ACTOR_VAR2:
				actor->var2 = TFE_Endian::le16_to_cpu(chunk[0]);
				break;

			case CF_CMD_ACTOR_CLIP:
				lrect_set(&actor->frame, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]), TFE_Endian::le16_to_cpu(chunk[2]), TFE_Endian::le16_to_cpu(chunk[3]));
				break;

			case CF_CMD_ACTOR_CLIPV:
				lrect_set(&actor->frameVelocity, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]), TFE_Endian::le16_to_cpu(chunk[2]), TFE_Endian::le16_to_cpu(chunk[3]));
				break;

			case CF_CMD_ACTOR_SHOW:
				if (TFE_Endian::le16_to_cpu(chunk[0])) { lactor_show(actor); }
				else { lactor_hide(actor); }
				break;

			case CF_CMD_ACTOR_FLIP:
				lactor_setFlip(actor, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]));
				break;
		}
	}

	void cutsceneFilm_setViewToFilm(Film* film, FilmObject* filmObj, u8* data)
	{
		s16* chunk = (s16*)(data + filmObj->offset);
		u8*  byteChunk = (u8*)chunk;
		u16  type = TFE_Endian::le16_to_cpu(chunk[1]);

		switch (type)
		{
			case CF_CMD_VIEW_SETRGB:
			{
				u16 start = (u16)byteChunk[4];
				u16 stop = (u16)byteChunk[5];
				u16 r = (u16)byteChunk[6];
				u16 g = (u16)byteChunk[7];
				u16 b = (u16)byteChunk[8];

				lpalette_setDstColor(start, stop, r, g, b);
			} break;
			case CF_CMD_VIEW_DEFPAL:
			{
				lpalette_setDstColor(0, 255, 0, 0, 0);
				if (film->def_palette)
				{
					lpalette_setDstPal(film->def_palette);
				}
			} break;
			case CF_CMD_VIEW_FADE:
			{
				s16 fade = TFE_Endian::le16_to_cpu(chunk[2]);
				s16 colorFade = TFE_Endian::le16_to_cpu(chunk[3]);
				cutsceneFilm_setFade(fade, colorFade);
			} break;
		}
	}

	void cutsceneFilm_setPaletteToFilm(Film* film, FilmObject* filmObj, u8* data)
	{
		s16* chunk = (s16*)(data + filmObj->offset);
		if (TFE_Endian::le16_to_cpu(chunk[1]) == CF_CMD_PALETTE_SET)
		{
			lpalette_setDstPal((LPalette*)filmObj->data);
		}
	}

	void cutsceneFilm_setSoundToFilm(Film* film, FilmObject* filmObj, u8* data)
	{
		LSound* sound = (LSound*)filmObj->data;
		s16* chunk = (s16*)(data + filmObj->offset);
		u16  type = TFE_Endian::le16_to_cpu(chunk[1]);
		chunk += 2;

		switch (type)
		{
			case CF_CMD_SOUND_START:
			{
				if (sound->var2 == 1)
				{
					startSpeech(sound);
				}
				else
				{
					startSfx(sound);
				}
			} break;
			case CF_CMD_SOUND_STOP:
			{
				stopSound(sound);
			} break;
			case CF_CMD_SOUND_VOLUME:
			{
				setSoundVolume(sound, TFE_Endian::le16_to_cpu(chunk[0]));
			} break;
			case CF_CMD_SOUND_FADE:
			{
				setSoundFade(sound, TFE_Endian::le16_to_cpu(chunk[0]), TFE_Endian::le16_to_cpu(chunk[1]));
			} break;
			case CF_CMD_SOUND_VAR1:
			{
				sound->var1 = TFE_Endian::le16_to_cpu(chunk[0]);
				if (sound->var1 == 1)
				{
					setSoundKeep(sound);
				}
			} break;
			case CF_CMD_SOUND_VAR2:
			{
				sound->var2 = TFE_Endian::le16_to_cpu(chunk[0]);
			} break;
			case CF_CMD_SOUND_CMD:
			{
				if (TFE_Endian::le16_to_cpu(chunk[0]))
				{
					if (sound->var2 == 1)
					{
						startSpeech(sound);
					}
					else
					{
						startSfx(sound);
					}
				}

				if (TFE_Endian::le16_to_cpu(chunk[1])) { setSoundVolume(sound, TFE_Endian::le16_to_cpu(chunk[1])); }
				if (TFE_Endian::le16_to_cpu(chunk[2]) || TFE_Endian::le16_to_cpu(chunk[3]))
				{
					setSoundFade(sound, TFE_Endian::le16_to_cpu(chunk[2]), TFE_Endian::le16_to_cpu(chunk[3]));
				}
			} break;
			case CF_CMD_SOUND_CMD2:
			{
				if (TFE_Endian::le16_to_cpu(chunk[0]))
				{
					if (sound->var2 == 1)
					{
						startSpeech(sound);
					}
					else
					{
						startSfx(sound);
					}
				}

				if (TFE_Endian::le16_to_cpu(chunk[1])) { setSoundVolume(sound, TFE_Endian::le16_to_cpu(chunk[1])); }
				if (TFE_Endian::le16_to_cpu(chunk[2]) || TFE_Endian::le16_to_cpu(chunk[3]))
				{
					setSoundFade(sound, TFE_Endian::le16_to_cpu(chunk[2]), TFE_Endian::le16_to_cpu(chunk[3]));
				}

				if (TFE_Endian::le16_to_cpu(chunk[4])) { setSoundPan(sound, TFE_Endian::le16_to_cpu(chunk[4])); }
				if (TFE_Endian::le16_to_cpu(chunk[5]) || TFE_Endian::le16_to_cpu(chunk[6])) { setSoundPanFade(sound, TFE_Endian::le16_to_cpu(chunk[5]), TFE_Endian::le16_to_cpu(chunk[6])); }
			} break;
		}
	}
				
	void cutsceneFilm_setFade(s16 fade, s16 colorFade)
	{
		s_filmState.filmFade = (FadeType)fade;
		s_filmState.filmColorFade = (FadeColorType)colorFade;
	}

	void cutsceneFilm_clearFade()
	{
		s_filmState.filmFade = ftype_noFade;
		s_filmState.filmColorFade = fcolorType_noColorFade;
	}

	void cutsceneFilm_startFade(JBool paletteChange)
	{
		s16 lock = 1;
		if (s_filmState.filmColorFade == fcolorType_paletteColorFade && s_filmState.filmFade <= ftype_longSnapFade)
		{
			lock = 0;
		}

		lfade_startFull(s_filmState.filmFade, s_filmState.filmColorFade, paletteChange, 0, lock);
		cutsceneFilm_clearFade();
	}

	JBool cutsceneFilm_isFading()
	{
		return (s_filmState.filmFade == ftype_noFade && s_filmState.filmColorFade == fcolorType_noColorFade) ? JFALSE : JTRUE;
	}

	void cutsceneFilm_stepActor(Film* film, FilmObject* filmObj, u8* data)
	{
		LActor* actor = (LActor*)filmObj->data;
		if (actor && actor->updateFunc)
		{
			actor->updateFunc(actor);
		}

		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepFilmFrame(filmObj, data);
			while (!cutsceneFilm_isNextFrame(filmObj, data))
			{
				cutsceneFilm_setActorToFilm(filmObj, data);
				cutsceneFilm_stepFilmFrame(filmObj, data);
			}
		}
	}

	void cutsceneFilm_stepView(Film* film, FilmObject* filmObj, u8* data)
	{
		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepFilmFrame(filmObj, data);
			while (!cutsceneFilm_isNextFrame(filmObj, data))
			{
				cutsceneFilm_setViewToFilm(film, filmObj, data);
				cutsceneFilm_stepFilmFrame(filmObj, data);
			}
		}
	}

	void cutsceneFilm_stepPalette(Film* film, FilmObject* filmObj, u8* data)
	{
		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepFilmFrame(filmObj, data);
			while (!cutsceneFilm_isNextFrame(filmObj, data))
			{
				cutsceneFilm_setPaletteToFilm(film, filmObj, data);
				cutsceneFilm_stepFilmFrame(filmObj, data);
			}
		}
	}

	void cutsceneFilm_stepSound(Film* film, FilmObject* filmObj, u8* data)
	{
		if (cutsceneFilm_isTimeStamp(film, filmObj, data))
		{
			cutsceneFilm_stepFilmFrame(filmObj, data);
			while (!cutsceneFilm_isNextFrame(filmObj, data))
			{
				cutsceneFilm_setSoundToFilm(film, filmObj, data);
				cutsceneFilm_stepFilmFrame(filmObj, data);
			}
		}
	}
		
	void cutsceneFilm_stepObjects(Film* film)
	{
		if (!lfade_isActive())
		{
			lpalette_copyScreenToDest(0, 0, 255);
			lpalette_copyScreenToSrc(0, 0, 255);
		}

		u8** array = film->array;
		for (s32 i = 0; i < film->arraySize; i++)
		{
			FilmObject* filmObj = (FilmObject*)array[i];
			if (filmObj)
			{
				switch (filmObj->id)
				{
					case CF_FILE_VIEW:
						cutsceneFilm_stepView(film, filmObj, (u8*)(filmObj + 1));
						break;
					case CF_FILE_PALETTE:
						cutsceneFilm_stepPalette(film, filmObj, (u8*)(filmObj + 1));
						break;
					case CF_FILE_ACTOR:
						cutsceneFilm_stepActor(film, filmObj, (u8*)(filmObj + 1));
						break;
					case CF_FILE_SOUND:
						cutsceneFilm_stepSound(film, filmObj, (u8*)(filmObj + 1));
						break;
				}
			}
		}

		if (!lfade_isActive())
		{
			if (!lpalette_compareSrcDst())
			{
				if (cutsceneFilm_isFading())
				{
					cutsceneFilm_startFade(JTRUE);
				}
				else if (!lfade_isActive())
				{
					lfade_startColorFade(fcolorType_snapColorFade, JTRUE, 1, 0, 1);
				}
				else
				{
					lpalette_copyDstToScreen(0, 0, 255);
					lpalette_putScreenPal();
				}
			}
			else if (cutsceneFilm_isFading())
			{
				cutsceneFilm_startFade(JFALSE);
			}
		}

		film->curCell++;
	}

	void cutsceneFilm_update(Film* film)
	{
		cutsceneFilm_stepObjects(film);
	}

	void cutsceneFilm_updateCallbacks(s32 time)
	{
		Film* film = s_filmState.firstFilm;
		while (film)
		{
			if (film->callback)
			{
				film->callback(film, time);
			}
			film = film->next;
		}
	}

	void cutsceneFilm_start(Film* film)
	{
		cutsceneFilm_rewind(film);
		film->flags |= (CF_STATE_VISIBLE | CF_STATE_ACTIVE);
	}

	void cutsceneFilm_stop(Film* film)
	{
		film->flags &= ~(CF_STATE_VISIBLE | CF_STATE_ACTIVE);
	}

	void cutsceneFilm_rewind(Film* film)
	{
		cutsceneFilm_rewindObjects(film);
	}

	JBool cutsceneFilm_isActive(Film* film)
	{
		return (film->flags & CF_STATE_ACTIVE) ? JTRUE : JFALSE;
	}

	JBool cutsceneFilm_isLooping(Film* film)
	{
		return (film->flags & CF_STATE_REPEAT) ? JTRUE : JFALSE;
	}
	
	void cutsceneFilm_updateFilms(s32 time)
	{
		Film* film = s_filmState.firstFilm;
		while (film)
		{
			if (film->start == time)
			{
				cutsceneFilm_start(film);
				if (film->stop == time)
				{
					cutsceneFilm_stop(film);
				}
			}
			else if (film->stop == time)
			{
				cutsceneFilm_stop(film);
			}
			else if (!film->cellCount)
			{
				if (cutsceneFilm_isActive(film))
				{
					cutsceneFilm_stop(film);
				}
			}
			else if (film->curCell >= film->cellCount)
			{
				if (cutsceneFilm_isLooping(film))
				{
					cutsceneFilm_rewind(film);
				}
				else
				{
					cutsceneFilm_stop(film);
				}
			}
			else
			{
				if (film->updateFunc && cutsceneFilm_isActive(film))
				{
					film->updateFunc(film);
				}
			}

			film = film->next;
		}
	}

	void cutsceneFilm_getRelativePos(Film* film, LRect* rect, s16* x, s16* y)
	{
		*x = film->x - film->frameRect.left + rect->left;
		*y = film->y - film->frameRect.top  + rect->top;
	}

	void cutsceneFilm_drawFilms(JBool refresh)
	{
		Film* film = s_filmState.firstFilm;
		for (s32 i = 0; i < LVIEW_COUNT; i++)
		{
			LRect rect;
			lview_getFrame(i, &rect);
			if (lrect_isEmpty(&rect)) { continue; }

			Film* curFilm = film;
			while (curFilm)
			{
				LRect clipRect;
				if (curFilm->drawFunc && (curFilm->flags & CF_STATE_VISIBLE) && lview_clipObjToView(i, curFilm->zplane, &curFilm->frameRect, &rect, &clipRect))
				{
					JBool curRefresh = (curFilm->flags & CF_STATE_REFRESH) | (curFilm->flags & CF_STATE_REFRESHABLE) | refresh;
					lcanvas_setClip(&clipRect);

					s16 x, y;
					cutsceneFilm_getRelativePos(curFilm, &rect, &x, &y);
					curFilm->drawFunc(curFilm, &rect, &clipRect, x, y, curRefresh);
				}

				curFilm = curFilm->next;
			}
		}

		for (Film* curFilm = film; curFilm; curFilm = curFilm->next)
		{
			curFilm->flags &= ~CF_STATE_REFRESH;
		}
	}

	void cutsceneFilm_add(Film* film)
	{
		Film* curFilm = s_filmState.firstFilm;
		Film* lastFilm = nullptr;
		while (curFilm && curFilm->zplane > film->zplane)
		{
			lastFilm = curFilm;
			curFilm = curFilm->next;
		}

		film->next = curFilm;
		if (!lastFilm)
		{
			s_filmState.firstFilm = film;
		}
		else
		{
			lastFilm->next = film;
		}
	}

	void cutsceneFilm_remove(Film* film)
	{
		if (!film) { return; }

		Film* curFilm = s_filmState.firstFilm;
		Film* lastFilm = nullptr;
		while (curFilm && curFilm != film)
		{
			lastFilm = curFilm;
			curFilm = curFilm->next;
		}

		if (curFilm == film)
		{
			if (!lastFilm)
			{
				s_filmState.firstFilm = curFilm->next;
			}
			else
			{
				lastFilm->next = curFilm->next;
			}
			film->next = nullptr;
		}
	}
}  // TFE_DarkForces