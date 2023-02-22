#include "robject.h"
#include "objDef.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Memory/chunkedArray.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>
#include <vector>
#include <map>

namespace TFE_Jedi
{
	enum Identifier
	{
		IdAsset = 0,
		IdLight,
		IdName,
		IdOffset,
		IdColor0,
		IdColor1,
		IdInnerRadius,
		IdOuterRadius,
		IdDecay,
		IdAmp,
		IdCount,
		IdInvalid
	};
	static const char* c_ident[IdCount]=
	{
		"asset",	   // IdAsset
		"light",       // IdLight
		"name",        // IdName
		"offset",      // IdOffset
		"color0",      // IdColor0
		"color1",      // IdColor1
		"innerRadius", // IdInnerRadius
		"outerRadius", // IdOuterRadius
		"decay",       // IdDecay
		"amp",         // IdAmp
	};
	enum DefType
	{
		DefAsset = 0,
		DefLight,
		DefCount,
		DefInvalid
	};
	enum DefFlags
	{
		DFLAG_NONE  = 0,
		DFLAG_LIGHT = FLAG_BIT(0),
	};

	struct ObjDef
	{
		std::string assetName;
		u32 flags;
		Light light;
	};
	std::vector<ObjDef> s_objDef;
	std::map<std::string, s32> s_objDefMap;

	void setLightDefaults(Light& light)
	{
		light.pos = { 0.0f };
		light.radii = { 0.0f, 5.0f };
		light.color[0] = { 1.0f, 1.0f, 1.0f };
		light.color[1] = { 1.0f, 1.0f, 1.0f };
		light.decay = 1.0f;
		light.amp = 1.0f;
	}

	Identifier getIdentifier(const char* str)
	{
		Identifier id = IdInvalid;
		for (s32 i = 0; i < IdCount; i++)
		{
			if (strcasecmp(str, c_ident[i]) == 0)
			{
				id = Identifier(i);
				break;
			}
		}
		return id;
	}

	void readAssetValue(s32& assetId, Identifier id, s32 offset, const char* str)
	{
		switch (id)
		{
			case IdName:
			{
				assetId = objDef_addAsset(str);
			} break;
		}
	}

	void readLightValue(Light& light, Identifier id, s32 offset, const char* str)
	{
		char* endPtr = nullptr;
		switch (id)
		{
			case IdOffset:
			{
				if (offset < 3)
				{
					light.pos.m[offset] = (f32)strtod(str, &endPtr);
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Definition Parser", "Offset has too many arguments %d / 3: asset: '%s'", offset + 1, str);
				}
			} break;
			case IdColor0:
			{
				if (offset < 3)
				{
					light.color[0].m[offset] = (f32)strtod(str, &endPtr);
					// Color1 defaults to color0.
					light.color[1].m[offset] = light.color[0].m[offset];
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Definition Parser", "Color0 has too many arguments %d / 3: asset: '%s'", offset + 1, str);
				}
			} break;
			case IdColor1:
			{
				if (offset < 3)
				{
					light.color[1].m[offset] = (f32)strtod(str, &endPtr);
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Definition Parser", "Color1 has too many arguments %d / 3: asset: '%s'", offset + 1, str);
				}
			} break;
			case IdInnerRadius:
			{
				if (offset == 0)
				{
					light.radii.m[0] = (f32)strtod(str, &endPtr);
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Definition Parser", "InnerRadius should only have a single value: asset: '%s'", str);
				}
			} break;
			case IdOuterRadius:
			{
				if (offset == 0)
				{
					light.radii.m[1] = (f32)strtod(str, &endPtr);
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Definition Parser", "OuterRadius should only have a single value: asset: '%s'", str);
				}
			} break;
			case IdDecay:
			{
				if (offset == 0)
				{
					light.decay = (f32)strtod(str, &endPtr);
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Definition Parser", "Decay should only have a single value: asset: '%s'", str);
				}
			} break;
			case IdAmp:
			{
				if (offset == 0)
				{
					light.amp = (f32)strtod(str, &endPtr);
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Definition Parser", "Amplitude should only have a single value: asset: '%s'", str);
				}
			} break;
		}
	}

	void objDef_init()
	{
		s_objDef.clear();
		s_objDefMap.clear();
	}

	bool objDef_open(const char* fileName, FileStream& file)
	{
		// First check to see if it exists in a mod.
		FilePath filePath;
		if (TFE_Paths::getFilePath(fileName, &filePath))
		{
			return file.open(&filePath, Stream::MODE_READ);
		}
		
		// If not, then load directly.
		char path[TFE_MAX_PATH];
		sprintf(path, "Definitions/%s", fileName);
		if (!TFE_Paths::mapSystemPath(path))
		{
			sprintf(path, "%sDefinitions/%s", TFE_Paths::getPath(PATH_PROGRAM), fileName);
		}
		return file.open(path, Stream::MODE_READ);
	}

	// General definitions parser.
	void objDef_parseDefinitions(FileStream& file)
	{
		std::vector<char> buffer;
		size_t len = file.getSize();
		buffer.resize(len + 1);
		file.readBuffer(buffer.data(), (u32)len);
		file.close();

		TFE_Parser parser;
		parser.init(buffer.data(), len);
		parser.enableBlockComments();
		parser.enableScopeTokens();
		parser.addCommentString("//");
		parser.addCommentString("#");

		Light light;
		size_t bufferPos = 0;
		s32 scopeLevel   = 0;
		s32 offset       = 0;
		s32 assetId      = -1;
		Identifier curId   = IdInvalid;
		DefType defType    = DefInvalid;
		DefType subDefType = DefInvalid;

		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos, true);
			TokenList tokens;
			parser.tokenizeLine(line, tokens);

			const size_t count = tokens.size();
			for (size_t t = 0; t < tokens.size(); t++)
			{
				const char* str = tokens[t].c_str();
				if (str[0] == '{')
				{
					scopeLevel++;
					offset = 0;

					// New override.
					if (scopeLevel == 1)
					{
						if (curId == IdAsset)
						{
							assetId = -1;
							defType = DefAsset;
						}
						else
						{
							TFE_System::logWrite(LOG_ERROR, "Definition Parser", "Invalid definition type.");
							return;
						}
						curId = IdInvalid;
					}
					else if (scopeLevel == 2)
					{
						if (curId == IdLight)
						{
							setLightDefaults(light);
							subDefType = DefLight;
						}
						else
						{
							TFE_System::logWrite(LOG_ERROR, "Definition Parser", "Invalid definition sub-type.");
							return;
						}
						curId = IdInvalid;
					}
				}
				else if (str[0] == '}')
				{
					scopeLevel--;
					curId = IdInvalid;

					// Add the item.
					if (scopeLevel == 0)
					{
						defType = DefInvalid;
					}
					else if (scopeLevel == 1)
					{
						if (subDefType == DefLight)
						{
							objDef_addLight(assetId, light);
						}
						subDefType = DefInvalid;
					}
				}
				else if (curId == IdInvalid)  // this should be an identifier.
				{
					curId = getIdentifier(str);
					offset = 0;
					if (curId == IdInvalid)
					{
						TFE_System::logWrite(LOG_ERROR, "Definition Parser", "Invalid identifier '%s'.", str);
						return;
					}
				}
				else
				{
					if (subDefType == DefInvalid)
					{
						if (defType == DefAsset)
						{
							readAssetValue(assetId, curId, offset, str);
						}
					}
					else
					{
						if (subDefType == DefLight)
						{
							readLightValue(light, curId, offset, str);
						}
					}
					offset++;

					// Auto-line statement ending - when *not* scoping the statement values, end the statement with the newline.
					// Scoping values allows statements to extend across multiple lines.
					if ((scopeLevel == 2 && subDefType != DefInvalid) || (scopeLevel == 1 && defType != DefInvalid))
					{
						curId = IdInvalid;
					}
				}
			}
		}
	}

	void objDef_loadLightDef()
	{
		FileStream file;
		if (objDef_open("lights.txt", file))
		{
			objDef_parseDefinitions(file);
		}
	}

	void objDef_destroy()
	{
	}
		
	s32 objDef_getIndex(const char* assetName)
	{
		s32 id = -1;
		std::map<std::string, s32>::iterator iAsset = s_objDefMap.find(assetName);
		if (iAsset != s_objDefMap.end())
		{
			id = iAsset->second;
		}
		return id;
	}

	s32 objDef_addAsset(const char* assetName)
	{
		s32 id = objDef_getIndex(assetName);
		if (id < 0)
		{
			id = (s32)s_objDef.size();
			s_objDefMap[assetName] = id;
			s_objDef.push_back({ assetName, DFLAG_NONE, {} });
		}
		return id;
	}

	void objDef_addLight(s32 assetId, Light light)
	{
		if (assetId >= 0 && assetId < (s32)s_objDef.size())
		{
			s_objDef[assetId].flags |= DFLAG_LIGHT;
			s_objDef[assetId].light = light;
			return;
		}
		TFE_System::logWrite(LOG_ERROR, "Definition Parser", "Cannot add a light to an asset that doesn't exist %d.", assetId);
	}

	bool objDef_getLight(s32 index, s32 animId, s32 frameId, Light* light)
	{
		assert(light);
		if (index >= 0 && index < (s32)s_objDef.size())
		{
			if (s_objDef[index].flags & DFLAG_LIGHT)
			{
				*light = s_objDef[index].light;
				return true;
			}
		}
		return false;
	}
}