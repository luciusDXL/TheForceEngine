#include "cutscene_film.h"
#include "util.h"
#include "time.h"
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Archive/lfdArchive.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	Film* allocate()
	{
		Film* film = (Film*)game_alloc(sizeof(Film));
		if (!film) { return nullptr; }

		memset(film, 0, sizeof(Film));
		film->stop = -1;
		film->flags = CF_STATE_REFRESH;

		return film;
	}

	u32 swapEndian(u32 x)
	{
		u8* buffer = (u8*)&x;
		return u32(buffer[3]) | (u32(buffer[2])<<8) | (u32(buffer[1])<<16) | (u32(buffer[0])<<24);
	}

	JBool cutsceneFilm_loadResources(FileStream* file, u8** array, s16 arraySize)
	{
		for (s32 i = 0; i < arraySize; i++)
		{
			s32 type;
			file->read(&type);
			type = swapEndian(type);

			char name[32];
			file->readBuffer(name, 8);
			name[8] = 0;

			u32 size;
			s16 id, chunks, used;
			file->read(&size);
			file->read(&id);
			file->read(&chunks);
			file->read(&used);

			// Allocate space for the object.
			FilmObject* obj = (FilmObject*)game_alloc(sizeof(FilmObject) + used);
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

	JBool cutsceneFilm_readObject(u32 type, const char* name, u8** data)
	{
		JBool found = JFALSE;
		if (type != CF_TYPE_VIEW && type != CF_TYPE_CUSTOM_ACTOR)
		{
			if (type == CF_TYPE_DELTA_ACTOR)
			{
				found = JTRUE;
			}
			else if (type == CF_TYPE_ANIM_ACTOR && !found)
			{
				found = JTRUE;
			}
			else if (type == CF_TYPE_PALETTE && !found)
			{
				found = JTRUE;
			}
			else if (type == CF_TYPE_GMIDI && !found)
			{
				found = JTRUE;
			}
			else if (type == CF_TYPE_VOC_SOUND && !found)
			{
				found = JTRUE;
			}
		}
		else if (type == CF_TYPE_CUSTOM_ACTOR)
		{
			found = JTRUE;
		}

		if (!found)
		{
			static s32 _x = 0;
			_x++;
		}

		return JTRUE;
	}

	JBool cutsceneFilm_readObjects(Film* film, void* callback)
	{
		FilmCallback filmCallback = nullptr;// (FilmCallback)callback;

		for (s32 i = 0; i < film->arraySize; i++)
		{
			FilmObject* obj = (FilmObject*)film->array[i];

			// Read the object.
			if (cutsceneFilm_readObject(obj->resType, obj->name, &obj->data))
			{
				if (filmCallback && filmCallback(film, obj))
				{
					game_free(obj->data);
					game_free(obj);
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

	Film* cutsceneFilm_load(const char* name, LRect* frameRect, s16 x, s16 y, s16 z, void* callback)
	{
		Film* film = allocate();
		if (!film) { return nullptr; }

		u8** array = nullptr;
		FilePath resPath;
		if (TFE_Paths::getFilePath(name, &resPath))
		{
			FileStream file;
			file.open(&resPath, FileStream::MODE_READ);

			s16 version;
			s16 cellCount;
			s16 arraySize;
			file.read(&version);
			file.read(&cellCount);
			file.read(&arraySize);

			if (version != CF_VERSION)
			{
				file.close();
				game_free(film);
				return nullptr;
			}

			array = (u8**)game_alloc(sizeof(u8*) * arraySize);
			memset(array, 0, sizeof(u8*) * arraySize);
			if (array)
			{
				film->array = array;
				film->arraySize = arraySize;
				film->curCell = 0;
				film->cellCount = cellCount;

				if (!cutsceneFilm_loadResources(&file, film->array, film->arraySize))
				{
					game_free(film);
					game_free(array);
					film = nullptr;
				}
			}
			file.close();

			if (!cutsceneFilm_readObjects(film, callback))
			{
				game_free(film);
				game_free(array);
				film = nullptr;
				array = nullptr;
			}
		}

		return film;
	}
}  // TFE_DarkForces