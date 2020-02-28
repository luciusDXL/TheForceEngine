#include "levelObjectsAsset.h"
#include <DXL2_System/system.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_System/parser.h>
#include <DXL2_FileSystem/filestream.h>
#include <assert.h>
#include <algorithm>

namespace DXL2_LevelObjects
{
	static std::vector<char> s_buffer;
	static LevelObjectData s_data = {};
	static const char* c_defaultGob = "DARK.GOB";
	static s32 s_curSectorIndex = -1;

	bool parseObjects();
	ObjectClass getObjectClass(const char* name);
	LogicType getLogicType(const char* name);

	bool load(const char* name)
	{
		char gobPath[DXL2_MAX_PATH];
		DXL2_Paths::appendPath(PATH_SOURCE_DATA, c_defaultGob, gobPath);
		// Unload the current level.
		unload();

		if (!DXL2_AssetSystem::readAssetFromGob(c_defaultGob, name, s_buffer))
		{
			return false;
		}

		// Parse the file...
		if (!parseObjects())
		{
			DXL2_System::logWrite(LOG_ERROR, "Level", "Failed to parse O file \"%s\" from GOB \"%s\"", name, gobPath);

			unload();
			return false;
		}
		
		return true;
	}

	void unload()
	{
		s_data.pods.clear();
		s_data.sprites.clear();
		s_data.frames.clear();
		s_data.sounds.clear();

		s_data.objectCount = 0;
		s_data.objects.clear();
	}

	void save(const char* name, const char* path)
	{
		FileStream outFile;
		if (!outFile.open(path, FileStream::MODE_WRITE))
		{
			return;
		}

		outFile.writeString("O 1.1\r\n\r\n");
		outFile.writeString("/* ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~\r\n");
		outFile.writeString("This file has been created by DarkXL 2 from %s.\r\n\r\n", name);
		outFile.writeString("~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~*/\r\n\r\n");

		outFile.writeString("LEVELNAME %s\r\n\r\n", name);

		const u32 podCount = (u32)s_data.pods.size();
		const std::string* pod = s_data.pods.data();
		outFile.writeString("PODS %u\r\n", podCount);
		for (u32 i = 0; i < podCount; i++, pod++)
		{
			outFile.writeString("    POD: %s\r\n", pod->c_str());
		}

		const u32 spriteCount = (u32)s_data.sprites.size();
		const std::string* sprite = s_data.sprites.data();
		outFile.writeString("\r\nSPRS %u\r\n", spriteCount);
		for (u32 i = 0; i < spriteCount; i++, sprite++)
		{
			outFile.writeString("    SPR: %s\r\n", sprite->c_str());
		}

		const u32 fmeCount = (u32)s_data.frames.size();
		const std::string* frame = s_data.frames.data();
		outFile.writeString("\r\nFMES %u\r\n", fmeCount);
		for (u32 i = 0; i < fmeCount; i++, frame++)
		{
			outFile.writeString("    FME: %s\r\n", frame->c_str());
		}

		const u32 sndCount = (u32)s_data.sounds.size();
		const std::string* sound = s_data.sounds.data();
		outFile.writeString("\r\nSOUNDS %u\r\n", sndCount);
		for (u32 i = 0; i < sndCount; i++, sound++)
		{
			outFile.writeString("    SOUND: %s\r\n", sound->c_str());
		}

		const u32 objCount = (u32)s_data.objects.size();
		const LevelObject* obj = s_data.objects.data();
		outFile.writeString("\r\nOBJECTS %u\r\n", objCount);
		for (u32 i = 0; i < objCount; i++, obj++)
		{
			outFile.writeString("    CLASS: %s	DATA: %u	X: %2.2f Y: %2.2f Z: %2.2f PCH: %2.2f YAW: %2.2f ROL: %2.2f	DIFF: %u\r\n",
				c_objectClassName[obj->oclass], obj->dataOffset, obj->pos.x, obj->pos.y, obj->pos.z, obj->orientation.x, obj->orientation.y, obj->orientation.z, obj->difficulty);
			outFile.writeString("        SEQ\r\n");

			// Logics
			const u32 logicCount = (u32)obj->logics.size();
			const Logic* logic = obj->logics.data();
			bool isScenery = false;
			for (u32 l = 0; l < logicCount; l++, logic++)
			{
				outFile.writeString("            LOGIC:	%s\r\n", c_logicName[logic->type]);
				if (logic->type == LOGIC_UPDATE)
				{
					outFile.writeString("            FLAGS:	%u\r\n", logic->flags);
					if (logic->flags & 8)  { outFile.writeString("            D_PITCH:	%2.2f\r\n", logic->rotation.x); }
					if (logic->flags & 16) { outFile.writeString("            D_YAW:	%2.2f\r\n", logic->rotation.y); }
					if (logic->flags & 32) { outFile.writeString("            D_ROLL:	%2.2f\r\n", logic->rotation.z); }
				}
				else if (logic->type == LOGIC_KEY)
				{
					outFile.writeString("            VUE: %s \"%s\"\r\n", logic->Vue.c_str(), logic->VueId.c_str());
					if (!logic->VueAppend.empty())
					{
						outFile.writeString("            VUE_APPEND: %s \"%s\"\r\n", logic->VueAppend.c_str(), logic->VueAppendId.c_str());
					}
					outFile.writeString("            PAUSE: %s", obj->comFlags & LCF_PAUSE ? "TRUE" : "FALSE");
					if (logic->frameRate != 0.0f) { outFile.writeString("            FRAME_RATE: %2.2f", logic->frameRate); }
				}
				else if (logic->type == LOGIC_SCENERY)
				{
					isScenery = true;
				}
			}
			
			// Generators
			const u32 genCount = (u32)obj->generators.size();
			const EnemyGenerator* gen = obj->generators.data();
			for (u32 l = 0; l < genCount; l++, gen++)
			{
				outFile.writeString("            LOGIC:	GENERATOR %s\r\n", c_logicName[gen->type]);
				outFile.writeString("            DELAY: %2.2f\r\n", gen->delay);
				outFile.writeString("            INTERVAL : %2.2f\r\n", gen->interval);
				outFile.writeString("            MIN_DIST : %2.2f\r\n", gen->minDist);
				outFile.writeString("            MAX_DIST : %2.2f\r\n", gen->maxDist);
				outFile.writeString("            MAX_ALIVE : %d\r\n", gen->maxAlive);
				outFile.writeString("            NUM_TERMINATE : %d\r\n", gen->numTerminate);
				outFile.writeString("            WANDER_TIME : 2.2f\r\n", gen->wanderTime);
			}

			// Variables
			if (obj->comFlags & LCF_EYE)  { outFile.writeString("            EYE: TRUE\r\n"); }
			if (obj->comFlags & LCF_BOSS) { outFile.writeString("            BOSS: TRUE\r\n"); }
			if (obj->radius >= 0.0f)
			{
				outFile.writeString("            RADIUS: %2.2f\r\n", obj->radius);
			}
			if (obj->height != 0.0f)
			{
				outFile.writeString("            HEIGHT: %2.2f\r\n", obj->height);
			}

			outFile.writeString("        SEQEND\r\n");
		}
		outFile.close();
	}

	LevelObjectData* getLevelObjectData()
	{
		return &s_data;
	}

	bool parseObjects()
	{
		if (s_buffer.empty()) { return false; }

		const size_t len = s_buffer.size();

		DXL2_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), len);
		parser.addCommentString("//");
		parser.addCommentString("#");
		parser.enableBlockComments();

		u32 objectIndex = 0;
		u32 podIndex=0, sprIndex=0, fmeIndex=0, sndIndex=0;
		LevelObject* object = nullptr;
		Logic* logic = nullptr;
		EnemyGenerator* generator = nullptr;
		bool sequence = false;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }

			char* endPtr = nullptr;
			if (strcasecmp("PODS", tokens[0].c_str()) == 0)
			{
				s_data.pods.resize(strtoul(tokens[1].c_str(), &endPtr, 10));
			}
			else if (strcasecmp("POD:", tokens[0].c_str()) == 0)
			{
				s_data.pods[podIndex++] = tokens.size() > 1 ? tokens[1] : "";
			}
			else if (strcasecmp("SPRS", tokens[0].c_str()) == 0)
			{
				s_data.sprites.resize(strtoul(tokens[1].c_str(), &endPtr, 10));
			}
			else if (strcasecmp("SPR:", tokens[0].c_str()) == 0)
			{
				s_data.sprites[sprIndex++] = tokens.size() > 1 ? tokens[1] : "";
			}
			else if (strcasecmp("FMES", tokens[0].c_str()) == 0)
			{
				s_data.frames.resize(strtoul(tokens[1].c_str(), &endPtr, 10));
			}
			else if (strcasecmp("FME:", tokens[0].c_str()) == 0)
			{
				s_data.frames[fmeIndex++] = tokens.size() > 1 ? tokens[1] : "";
			}
			else if (strcasecmp("SOUNDS", tokens[0].c_str()) == 0)
			{
				s_data.sounds.resize(strtoul(tokens[1].c_str(), &endPtr, 10));
			}
			else if (strcasecmp("SOUND:", tokens[0].c_str()) == 0)
			{
				s_data.sounds[sndIndex++] = tokens.size() > 1 ? tokens[1] : "";
			}
			else if (strcasecmp("OBJECTS", tokens[0].c_str()) == 0)
			{
				s_data.objectCount = strtoul(tokens[1].c_str(), &endPtr, 10);
				s_data.objects.resize(s_data.objectCount);
			}
			else if (strcasecmp("CLASS:", tokens[0].c_str()) == 0)
			{
				object = &s_data.objects[objectIndex];
				objectIndex++;

				object->oclass = getObjectClass(tokens[1].c_str());
				const u32 tokenCount = (u32)tokens.size();
				for (u32 t = 2; t < tokenCount; t++)
				{
					if (strcasecmp(tokens[t].c_str(), "DATA:") == 0)
					{
						t++;
						object->dataOffset = strtoul(tokens[t].c_str(), &endPtr, 10);
					}
					else if (strcasecmp(tokens[t].c_str(), "X:") == 0)
					{
						t++;
						object->pos.x = (f32)strtod(tokens[t].c_str(), &endPtr);
					}
					else if (strcasecmp(tokens[t].c_str(), "Y:") == 0)
					{
						t++;
						object->pos.y = (f32)strtod(tokens[t].c_str(), &endPtr);
					}
					else if (strcasecmp(tokens[t].c_str(), "Z:") == 0)
					{
						t++;
						object->pos.z = (f32)strtod(tokens[t].c_str(), &endPtr);
					}
					else if (strcasecmp(tokens[t].c_str(), "PCH:") == 0)
					{
						t++;
						object->orientation.x = (f32)strtod(tokens[t].c_str(), &endPtr);
					}
					else if (strcasecmp(tokens[t].c_str(), "YAW:") == 0)
					{
						t++;
						object->orientation.y = (f32)strtod(tokens[t].c_str(), &endPtr);
					}
					else if (strcasecmp(tokens[t].c_str(), "ROL:") == 0)
					{
						t++;
						object->orientation.z = (f32)strtod(tokens[t].c_str(), &endPtr);
					}
					else if (strcasecmp(tokens[t].c_str(), "DIFF:") == 0)
					{
						t++;
						object->difficulty = strtol(tokens[t].c_str(), &endPtr, 10);
					}
				}
			}
			else if (strcasecmp("SEQ", tokens[0].c_str()) == 0)
			{
				sequence = true;
			}
			else if (strcasecmp("SEQEND", tokens[0].c_str()) == 0)
			{
				sequence = false;
				object = nullptr;
				logic = nullptr;
				generator = nullptr;
			}
			else if (sequence)
			{
				if (strcasecmp("LOGIC:", tokens[0].c_str()) == 0 || strcasecmp("TYPE:", tokens[0].c_str()) == 0)
				{
					if (strcasecmp("GENERATOR", tokens[1].c_str()) == 0)
					{
						size_t index = object->generators.size();
						object->generators.push_back({});
						generator = &object->generators[index];
						if (tokens.size() >= 3)
						{
							generator->type = getLogicType(tokens[2].c_str());
						}
						else
						{
							// Unknown effect.
							generator->type = LOGIC_COUNT;
						}
					}
					else
					{
						size_t index = object->logics.size();
						object->logics.push_back({});
						logic = &object->logics[index];

						// ITEM XXX is just referenced as XXX since "ITEM SHIELD" is the same as "SHIELD" and they are used interchangably (and similar for other items).
						std::string logicName = tokens[1];
						if (tokens.size() == 3 && strcasecmp(logicName.c_str(), "ITEM") == 0)
						{
							logicName = tokens[2];
						}
						logic->type = getLogicType(logicName.c_str());
					}
				}
				// Generator
				else if (generator && strcasecmp("DELAY:", tokens[0].c_str()) == 0)
				{
					generator->delay = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (generator && strcasecmp("INTERVAL:", tokens[0].c_str()) == 0)
				{
					generator->interval = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (generator && strcasecmp("MIN_DIST:", tokens[0].c_str()) == 0)
				{
					generator->minDist = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (generator && strcasecmp("MAX_DIST:", tokens[0].c_str()) == 0)
				{
					generator->maxDist = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (generator && strcasecmp("MAX_ALIVE:", tokens[0].c_str()) == 0)
				{
					generator->maxAlive = strtoul(tokens[1].c_str(), &endPtr, 10);
				}
				else if (generator && strcasecmp("MAX_ALIVE:", tokens[0].c_str()) == 0)
				{
					generator->numTerminate = strtoul(tokens[1].c_str(), &endPtr, 10);
				}
				else if (generator && strcasecmp("WANDER_TIME:", tokens[0].c_str()) == 0)
				{
					generator->wanderTime = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				// Logic
				else if (strcasecmp("EYE:", tokens[0].c_str()) == 0 && strcasecmp(tokens[1].c_str(), "TRUE") == 0)
				{
					object->comFlags |= LCF_EYE;
				}
				else if (strcasecmp("BOSS:", tokens[0].c_str()) == 0 && strcasecmp(tokens[1].c_str(), "TRUE") == 0)
				{
					object->comFlags |= LCF_BOSS;
				}
				else if (strcasecmp("PAUSE:", tokens[0].c_str()) == 0 && strcasecmp(tokens[1].c_str(), "TRUE") == 0)
				{
					object->comFlags |= LCF_PAUSE;
				}
				else if (logic && strcasecmp("FLAGS:", tokens[0].c_str()) == 0)
				{
					logic->flags = strtoul(tokens[1].c_str(), &endPtr, 10);
				}
				else if (strcasecmp("RADIUS:", tokens[0].c_str()) == 0)
				{
					object->radius = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (strcasecmp("HEIGHT:", tokens[0].c_str()) == 0)
				{
					object->height = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (logic && strcasecmp("FRAMERATE:", tokens[0].c_str()) == 0)
				{
					logic->frameRate = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (logic && strcasecmp("D_PITCH:", tokens[0].c_str()) == 0)
				{
					logic->rotation.x = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (logic && strcasecmp("D_YAW:", tokens[0].c_str()) == 0)
				{
					logic->rotation.y = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (logic && strcasecmp("D_ROLL:", tokens[0].c_str()) == 0)
				{
					logic->rotation.z = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else if (logic && strcasecmp("VUE:", tokens[0].c_str()) == 0)
				{
					logic->Vue = tokens[1];
					logic->VueId = tokens[2];
				}
				else if (logic && strcasecmp("VUE_APPEND:", tokens[0].c_str()) == 0)
				{
					logic->VueAppend = tokens[1];
					logic->VueAppendId = tokens[2];
				}
			}
		}
		return true;
	}

	ObjectClass getObjectClass(const char* name)
	{
		for (u32 i = 0; i < CLASS_COUNT; i++)
		{
			if (strcasecmp(name, c_objectClassName[i]) == 0)
			{
				return ObjectClass(i);
			}
		}
		return CLASS_INAVLID;
	}

	LogicType getLogicType(const char* name)
	{
		for (u32 i = 0; i < LOGIC_COUNT; i++)
		{
			if (strcasecmp(name, c_logicName[i]) == 0)
			{
				return LogicType(i);
			}
		}
		assert(0);
		return LOGIC_INVALID;
	}
};
