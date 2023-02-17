#include "robject.h"
#include "objOverrides.h"
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
		IdName = 0,
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
		"name",        // IdName = 0,
		"offset",      // IdOffset,
		"color0",      // IdColor0,
		"color1",      // IdColor1,
		"innerRadius", // IdInnerRadius,
		"outerRadius", // IdOuterRadius,
		"decay",       // IdDecay,
		"amp",         // IdAmp,
	};

	struct LightOverride
	{
		std::string assetName;
		Light light;
	};
	std::vector<LightOverride> s_overrides;
	std::map<std::string, s32> s_overrideMap;

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

	void readValue(Light& light, char* assetName, Identifier id, s32 offset, const char* str)
	{
		char* endPtr = nullptr;
		switch (id)
		{
			case IdName:
			{
				strcpy(assetName, str);
			} break;
			case IdOffset:
			{
				if (offset < 3)
				{
					light.pos.m[offset] = (f32)strtod(str, &endPtr);
				}
				else
				{
					TFE_System::logWrite(LOG_WARNING, "Override Parser", "Offset has too many arguments %d / 3: asset: '%s'", offset + 1, str);
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
					TFE_System::logWrite(LOG_WARNING, "Override Parser", "Color0 has too many arguments %d / 3: asset: '%s'", offset + 1, str);
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
					TFE_System::logWrite(LOG_WARNING, "Override Parser", "Color1 has too many arguments %d / 3: asset: '%s'", offset + 1, str);
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
					TFE_System::logWrite(LOG_WARNING, "Override Parser", "InnerRadius should only have a single value: asset: '%s'", str);
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
					TFE_System::logWrite(LOG_WARNING, "Override Parser", "OuterRadius should only have a single value: asset: '%s'", str);
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
					TFE_System::logWrite(LOG_WARNING, "Override Parser", "Decay should only have a single value: asset: '%s'", str);
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
					TFE_System::logWrite(LOG_WARNING, "Override Parser", "Amplitude should only have a single value: asset: '%s'", str);
				}
			} break;
		}
	}

	void objOverrides_init()
	{
		s_overrides.clear();
		s_overrideMap.clear();

		// Read from disk.
		char path[TFE_MAX_PATH];
		const char* programDir = TFE_Paths::getPath(PATH_PROGRAM);
		sprintf(path, "%sMods/TFE/Lights/lightOverrides.txt", programDir);

		std::vector<char> buffer;
		size_t len = 0u;
		FileStream file;
		if (file.open(path, Stream::MODE_READ))
		{
			len = file.getSize();
			buffer.resize(len + 1);
			file.readBuffer(buffer.data(), len);
			file.close();
		}

		TFE_Parser parser;
		parser.init(buffer.data(), len);
		parser.enableBlockComments();
		parser.enableScopeTokens();
		parser.addCommentString("//");
		parser.addCommentString("#");

		Light light;
		size_t bufferPos = 0;
		s32 scopeLevel = 0;	// global
		s32 offset = 0;
		char assetName[256] = { 0 };
		Identifier curId = IdInvalid;
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
						setLightDefaults(light);
						curId = IdInvalid;
						memset(assetName, 0, 256);
					}
				}
				else if (str[0] == '}')
				{
					scopeLevel--;
					curId = IdInvalid;

					// Add the item.
					if (scopeLevel == 0)
					{
						objOverrides_addLight(assetName, light);
					}
				}
				else if (curId == IdInvalid)  // this should be an identifier.
				{
					curId = getIdentifier(str);
					offset = 0;
					if (curId == IdInvalid)
					{
						TFE_System::logWrite(LOG_ERROR, "Override Parser", "Invalid identifier '%s'.", str);
						return;
					}
				}
				else
				{
					readValue(light, assetName, curId, offset, str);
					offset++;
				}
			}

			// If we are at the end of a line and the scope == 1, then treat this as the end of the current value.
			if (scopeLevel == 1)
			{
				curId = IdInvalid;
			}
		}
	}

	void objOverrides_destroy()
	{
	}
		
	s32 objOverrides_getIndex(const char* assetName)
	{
		s32 id = -1;
		std::map<std::string, s32>::iterator iAsset = s_overrideMap.find(assetName);
		if (iAsset != s_overrideMap.end())
		{
			id = iAsset->second;
		}
		return id;
	}

	s32 objOverrides_addLight(const char* assetName, Light light)
	{
		s32 id = -1;

		std::map<std::string, s32>::iterator iAsset = s_overrideMap.find(assetName);
		if (iAsset == s_overrideMap.end())
		{
			id = (s32)s_overrides.size();
			s_overrideMap[assetName] = id;
		}
		else
		{
			id = iAsset->second;
		}

		s_overrides.push_back({ assetName, light });
		return id;
	}

	void objOverrides_getLight(s32 index, Light* light)
	{
		assert(light);
		if (index >= 0 && index < (s32)s_overrides.size())
		{
			*light = s_overrides[index].light;
		}
		else
		{
			memset(light, 0, sizeof(Light));
		}
	}
}