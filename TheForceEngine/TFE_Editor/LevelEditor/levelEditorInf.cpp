#include "levelEditorInf.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "entity.h"
#include "selection.h"
#include "infoPanel.h"
#include "browser.h"
#include "camera.h"
#include "error.h"
#include "sharedState.h"
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editorLevel.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_Editor/editorResources.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Editor/LevelEditor/Rendering/grid2d.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/lineDraw3d.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>
#include <SDL.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	Editor_LevelInf s_levelInf;
	static char s_infArg0[256], s_infArg1[256], s_infArg2[256], s_infArg3[256], s_infArg4[256], s_infArgExtra[256];

	const char* c_infElevTypeName[] =
	{
		// Special, "high level" elevators.
		"Basic",		// IET_BASIC
		"Basic_Auto",	// IET_BASIC_AUTO
		"Inv",			// IET_INV
		"Door",			// IET_DOOR
		"Door_Inv",		// IET_DOOR_INV
		"Door_Mid",		// IET_DOOR_MID
		"Morph_Spin1",	// IET_MORPH_SPIN1
		"Morph_Spin2",	// IET_MORPH_SPIN2
		"Morph_Move1",	// IET_MORPH_MOVE1
		"Morph_Move1",	// IET_MORPH_MOVE2

		// Standard Elevators
		"Move_Ceiling",			// IET_MOVE_CEILING
		"Move_Floor",			// IET_MOVE_FLOOR
		"Move_FC",				// IET_MOVE_FC
		"Move_Offset",			// IET_MOVE_OFFSET
		"Move_Wall",			// IET_MOVE_WALL
		"Rotate_Wall",			// IET_ROTATE_WALL
		"Scroll_Wall",			// IET_SCROLL_WALL
		"Scroll_Floor",			// IET_SCROLL_FLOOR
		"Scroll_Ceiling",		// IET_SCROLL_CEILING
		"Change_Light",			// IET_CHANGE_LIGHT
		"Change_Wall_Light",	// IET_CHANGE_WALL_LIGHT
	};

	void editor_infInit()
	{
		s_levelInf.item.clear();
		s_levelInf.elevator.clear();
		s_levelInf.trigger.clear();
		s_levelInf.teleport.clear();
	}

	void editor_infDestroy()
	{
		s32 count = (s32)s_levelInf.elevator.size();
		for (s32 i = 0; i < count; i++)
		{
			delete s_levelInf.elevator[i];
		}

		count = (s32)s_levelInf.trigger.size();
		for (s32 i = 0; i < count; i++)
		{
			delete s_levelInf.trigger[i];
		}

		count = (s32)s_levelInf.teleport.size();
		for (s32 i = 0; i < count; i++)
		{
			delete s_levelInf.teleport[i];
		}

		editor_infInit();
	}

	Editor_InfElevator* allocElev(Editor_InfItem* item)
	{
		Editor_InfElevator* elev = new Editor_InfElevator();
		s_levelInf.elevator.push_back(elev);
		elev->classId = IIC_ELEVATOR;
		item->classData.push_back((Editor_InfClass*)elev);
		return elev;
	}

	Editor_InfTrigger* allocTrigger(Editor_InfItem* item)
	{
		Editor_InfTrigger* trigger = new Editor_InfTrigger();
		s_levelInf.trigger.push_back(trigger);
		trigger->classId = IIC_TRIGGER;
		item->classData.push_back((Editor_InfClass*)trigger);
		return trigger;
	}

	Editor_InfTeleporter* allocTeleporter(Editor_InfItem* item)
	{
		Editor_InfTeleporter* teleport = new Editor_InfTeleporter();
		s_levelInf.teleport.push_back(teleport);
		teleport->classId = IIC_TELEPORTER;
		item->classData.push_back((Editor_InfClass*)teleport);
		return teleport;
	}

	void freeElevator(Editor_InfElevator* elev)
	{
		const s32 count = (s32)s_levelInf.elevator.size();
		s32 index = -1;
		for (s32 i = 0; i < count; i++)
		{
			if (s_levelInf.elevator[i] == elev)
			{
				index = i;
				break;
			}
		}
		if (index < 0) { return; }
		delete elev;
		for (s32 i = index; i < count - 1; i++)
		{
			s_levelInf.elevator[i] = s_levelInf.elevator[i + 1];
		}
		s_levelInf.elevator.pop_back();
	}

	void freeTrigger(Editor_InfTrigger* trigger)
	{
		const s32 count = (s32)s_levelInf.trigger.size();
		s32 index = -1;
		for (s32 i = 0; i < count; i++)
		{
			if (s_levelInf.trigger[i] == trigger)
			{
				index = i;
				break;
			}
		}
		if (index < 0) { return; }
		delete trigger;
		for (s32 i = index; i < count - 1; i++)
		{
			s_levelInf.trigger[i] = s_levelInf.trigger[i + 1];
		}
		s_levelInf.trigger.pop_back();
	}

	void freeTeleporter(Editor_InfTeleporter* teleporter)
	{
		const s32 count = (s32)s_levelInf.teleport.size();
		s32 index = -1;
		for (s32 i = 0; i < count; i++)
		{
			if (s_levelInf.teleport[i] == teleporter)
			{
				index = i;
				break;
			}
		}
		if (index < 0) { return; }
		delete teleporter;
		for (s32 i = index; i < count - 1; i++)
		{
			s_levelInf.teleport[i] = s_levelInf.teleport[i + 1];
		}
		s_levelInf.teleport.pop_back();
	}

	Editor_InfElevator* getElevFromClassData(Editor_InfClass* data)
	{
		if (data->classId != IIC_ELEVATOR) { return nullptr; }
		return (Editor_InfElevator*)data;
	}

	Editor_InfTrigger* getTriggerFromClassData(Editor_InfClass* data)
	{
		if (data->classId != IIC_TRIGGER) { return nullptr; }
		return (Editor_InfTrigger*)data;
	}

	Editor_InfTeleporter* getTeleportFromClassData(Editor_InfClass* data)
	{
		if (data->classId != IIC_TELEPORTER) { return nullptr; }
		return (Editor_InfTeleporter*)data;
	}

	EditorSector* findSector(const char* itemName)
	{
		const s32 count = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < count; i++, sector++)
		{
			// Note in vanilla DF, only the first 16 characters are compared.
			if (strncasecmp(itemName, sector->name.c_str(), 16) == 0)
			{
				return sector;
			}
		}
		return nullptr;
	}

	void editor_parseMessage(Editor_InfMessageType* type, u32* arg1, u32* arg2, u32* evt, const char* infArg0, const char* infArg1, const char* infArg2, bool elevator)
	{
		const KEYWORD msgId = getKeywordIndex(infArg0);
		char* endPtr = nullptr;

		switch (msgId)
		{
			case KW_NEXT_STOP:
				*type = IMT_NEXT_STOP;
				break;
			case KW_PREV_STOP:
				*type = IMT_PREV_STOP;
				break;
			case KW_GOTO_STOP:
				*type = IMT_GOTO_STOP;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_MASTER_ON:
				*type = IMT_MASTER_ON;
				break;
			case KW_MASTER_OFF:
				*type = IMT_MASTER_OFF;
				break;
			case KW_SET_BITS:
				*type = IMT_SET_BITS;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				*arg2 = strtoul(infArg2, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_CLEAR_BITS:
				*type = IMT_CLEAR_BITS;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				*arg2 = strtoul(infArg2, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_COMPLETE:
				*type = IMT_COMPLETE;
				*arg1 = strtoul(infArg1, &endPtr, 10);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_LIGHTS:
				*type = IMT_LIGHTS;
				break;
			case KW_M_TRIGGER:
			default:
				if (elevator)
				{
					// Elevators can use a few additional message types, but these are treated as M_TRIGGER for either trigger type.
					switch (msgId)
					{
						case KW_DONE:
							*type = IMT_DONE;
							break;
						case KW_WAKEUP:
							*type = IMT_WAKEUP;
							break;
						default:
							*type = IMT_TRIGGER;
					}
				}
				else // Trigger
				{
					*type = IMT_TRIGGER;
				}
		}
	}

	bool editor_parseElevatorCommand(s32 argCount, KEYWORD action, bool seqEnd, Editor_InfElevator* elev, s32& addon)
	{
		char* endPtr = nullptr;
		switch (action)
		{
			case KW_START:
			{
				elev->start = strtol(s_infArg0, &endPtr, 10);
				elev->overrideSet |= IEO_START;
			} break;
			case KW_STOP:
			{
				elev->stops.push_back({});
				Editor_InfStop* stop = &elev->stops.back();
				stop->overrideSet = ISO_NONE;

				// Calculate the stop value.
				if (s_infArg0[0] == '@')  // Relative Value
				{
					stop->relative = true;
					stop->value = strtof(&s_infArg0[1], &endPtr);
				}
				else if ((s_infArg0[0] >= '0' && s_infArg0[0] <= '9') || s_infArg0[0] == '-' || s_infArg0[0] == '.')  // Numeric Value
				{
					stop->relative = false;
					stop->value = strtof(s_infArg0, &endPtr);
				}
				else  // Value from named sector.
				{
					stop->relative = false;
					stop->value = 0.0f;
					stop->fromSectorFloor = s_infArg0;
				}

				if (argCount < 3)
				{
					return seqEnd;
				}

				stop->overrideSet |= ISO_DELAY;
				stop->delay = 0.0f;
				if ((s_infArg1[0] >= '0' && s_infArg1[0] <= '9') || s_infArg1[0] == '-' || s_infArg1[0] == '.')
				{
					stop->delayType = SDELAY_SECONDS;
					stop->delay = strtof(s_infArg1, &endPtr);
				}
				else if (strcasecmp(s_infArg1, "HOLD") == 0)
				{
					stop->delayType = SDELAY_HOLD;
				}
				else if (strcasecmp(s_infArg1, "TERMINATE") == 0)
				{
					stop->delayType = SDELAY_TERMINATE;
				}
				else if (strcasecmp(s_infArg1, "COMPLETE") == 0)
				{
					stop->delayType = SDELAY_COMPLETE;
				}
				else
				{
					// Note: this is actually a bug in the original game, but some mods rely on it so...
					stop->delayType = SDELAY_PREV_VALUE;
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_SPEED:
			{
				elev->speed = strtof(s_infArg0, &endPtr);
				elev->overrideSet |= IEO_SPEED;
			} break;
			case KW_MASTER:
			{
				elev->master = false;
				elev->overrideSet |= IEO_MASTER;
			} break;
			case KW_ANGLE:
			{
				elev->angle = strtof(s_infArg0, &endPtr) * PI / 180.0f;
				elev->overrideSet |= IEO_ANGLE;
			} break;
			case KW_ADJOIN:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->adjoinCmd.push_back({});
					Editor_InfAdjoinCmd* adjoinCmd = &stop->adjoinCmd.back();

					adjoinCmd->sector0 = s_infArg1;
					adjoinCmd->sector1 = s_infArg3;
					adjoinCmd->wallIndex0 = strtol(s_infArg2, &endPtr, 10);
					adjoinCmd->wallIndex1 = strtol(s_infArg4, &endPtr, 10);
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_TEXTURE:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->textureCmd.push_back({});
					Editor_InfTextureCmd* texCmd = &stop->textureCmd.back();

					texCmd->donorSector = s_infArg2;
					texCmd->fromCeiling = s_infArg1[0] == 'C' || s_infArg1[0] == 'c';
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_SLAVE:
			{
				elev->slaves.push_back({});
				Editor_InfSlave* slave = &elev->slaves.back();

				slave->name = s_infArg0;
				slave->angleOffset = 0.0f;
				if (argCount > 2)
				{
					slave->angleOffset = strtof(s_infArg1, &endPtr);
				}
				elev->overrideSet |= IEO_SLAVES;
			} break;
			case KW_MESSAGE:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->msg.push_back({});
					Editor_InfMessage* msg = &stop->msg.back();
					msg->targetWall = -1;

					// There should be a variant of strstr() that returns a non-constant pointer, but in Visual Studio it is always constant.
					char* parenOpen = (char*)strstr(s_infArg1, "(");
					// This message targets a wall rather than a whole sector.
					if (parenOpen)
					{
						*parenOpen = 0;
						parenOpen++;

						char* parenClose = (char*)strstr(s_infArg1, ")");
						// This should never be true and this doesn't seem to be hit at runtime.
						// I wonder if this was meant to be { char* parenClose = (char*)strstr(parenOpen, ")"); } - which would make more sense.
						// Or it could have been check *before* the location at ")" was set to 0 above.
						if (parenClose)
						{
							*parenClose = 0;
						}
						// Finally parse the integer and set the wall index.
						msg->targetWall = strtol(parenOpen, &endPtr, 10);
					}
					msg->targetSector = s_infArg1;
					msg->type = IMT_TRIGGER;
					msg->eventFlags = INF_EVENT_NONE;

					if (argCount >= 5)
					{
						msg->eventFlags = strtoul(s_infArg0, &endPtr, 10);
					}
					if (argCount > 3)
					{
						editor_parseMessage(&msg->type, &msg->arg[0], &msg->arg[1], &msg->eventFlags, s_infArg2, s_infArg3, s_infArg4, true);
					}
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_EVENT_MASK:
			{
				if (s_infArg0[0] == '*')
				{
					elev->eventMask = INF_EVENT_ANY;	// everything
				}
				else
				{
					elev->eventMask = strtoul(s_infArg0, &endPtr, 10);
				}
				elev->overrideSet |= IEO_EVENT_MASK;
			} break;
			case KW_ENTITY_MASK:
			case KW_OBJECT_MASK:
			{
				// Entity_mask and Object_mask are buggy for elevators...
				if (s_infArg0[0] == '*')
				{
					elev->eventMask = INF_ENTITY_ANY;
				}
				else
				{
					elev->eventMask = strtoul(s_infArg0, &endPtr, 10);
				}
				elev->overrideSet |= IEO_ENTITY_MASK;
			} break;
			case KW_CENTER:
			{
				const f32 centerX = strtof(s_infArg0, &endPtr);
				const f32 centerZ = strtof(s_infArg1, &endPtr);
				elev->dirOrCenter = { centerX, centerZ };
				elev->overrideSet |= IEO_DIR;
			} break;
			case KW_KEY_COLON:
			{
				KEYWORD key = getKeywordIndex(s_infArg0);
				if (key == KW_RED)
				{
					elev->key[addon] = KEY_RED;
				}
				else if (key == KW_YELLOW)
				{
					elev->key[addon] = KEY_YELLOW;
				}
				else if (key == KW_BLUE)
				{
					elev->key[addon] = KEY_BLUE;
				}
				elev->overrideSet |= (addon > 0 ? IEO_KEY1 : IEO_KEY0);
			} break;
			// This entry is required for special cases, like IELEV_SP_DOOR_MID, where multiple elevators are created at once.
			// This way, we can modify the first created elevator as well as the last.
			// It allows allows us to modify previous classes... but this isn't recommended.
			case KW_ADDON:
			{
				addon = strtol(s_infArg0, &endPtr, 10);
			} break;
			case KW_FLAGS:
			{
				elev->flags = strtoul(s_infArg0, &endPtr, 10);
				elev->overrideSet |= IEO_FLAGS;
			} break;
			case KW_SOUND_COLON:
			{
				std::string soundName = s_infArg1;
				if (s_infArg1[0] >= '0' && s_infArg1[0] <= '9')
				{
					// Any numeric value means "no sound."
					soundName = "none";
				}

				// Determine which elevator sound to assign soundId to.
				s32 soundIdx = strtol(s_infArg0, &endPtr, 10) - 1;
				if (soundIdx >= 0 && soundIdx < 3)
				{
					elev->sounds[soundIdx] = soundName;
					elev->overrideSet |= (IEO_SOUND0 << soundIdx);
				}
			} break;
			case KW_PAGE:
			{
				s32 stopId = strtol(s_infArg0, &endPtr, 10);
				Editor_InfStop* stop = stopId >= 0 && stopId < (s32)elev->stops.size() ? &elev->stops[stopId] : nullptr;
				if (stop)
				{
					stop->page = s_infArg1;
				}
				elev->overrideSet |= IEO_STOPS;
			} break;
			case KW_SEQEND:
			{
				seqEnd = true;
			} break;
		}

		return seqEnd;
	}

	bool parseElevator(TFE_Parser& parser, size_t& bufferPos, const char* itemName, Editor_InfItem* item)
	{
		EditorSector* itemSector = findSector(itemName);
		if (!itemSector)
		{
			parser.readLine(bufferPos);
			return false;
		}
						
		KEYWORD itemSubclass = getKeywordIndex(s_infArg1);
		// Special classes - these map to the 11 core elevator types but have special defaults and/or automatically setup stops.
		Editor_InfElevType type;
		if (itemSubclass <= KW_MORPH_MOVE2)
		{
			switch (itemSubclass)
			{
			case KW_BASIC:
				type = IET_BASIC;
				break;
			case KW_BASIC_AUTO:
				type = IET_BASIC_AUTO;
				break;
			case KW_INV:
				type = IET_INV;
				break;
			case KW_DOOR:
				type = IET_DOOR;
				break;
			case KW_DOOR_INV:
				type = IET_DOOR_INV;
				break;
			case KW_DOOR_MID:
				type = IET_DOOR_MID;
				break;
			case KW_MORPH_SPIN1:
				type = IET_MORPH_SPIN1;
				break;
			case KW_MORPH_SPIN2:
				type = IET_MORPH_SPIN2;
				break;
			case KW_MORPH_MOVE1:
				type = IET_MORPH_MOVE1;
				break;
			case KW_MORPH_MOVE2:
				type = IET_MORPH_MOVE2;
				break;
			default:
				// Invalid type.
				LE_WARNING("Unsupported INF elevator type '%s' (ignored by Dark Forces).", s_infArg1);
				parser.readLine(bufferPos);
				return false;
			};
		}
		// One of the core 11 types.
		else
		{
			switch (itemSubclass)
			{
			case KW_MOVE_CEILING:
				type = IET_MOVE_CEILING;
				break;
			case KW_MOVE_FLOOR:
				type = IET_MOVE_FLOOR;
				break;
			case KW_MOVE_FC:
				type = IET_MOVE_FC;
				break;
			case KW_MOVE_OFFSET:
				type = IET_MOVE_OFFSET;
				break;
			case KW_MOVE_WALL:
				type = IET_MOVE_WALL;
				break;
			case KW_ROTATE_WALL:
				type = IET_ROTATE_WALL;
				break;
			case KW_SCROLL_WALL:
				type = IET_SCROLL_WALL;
				break;
			case KW_SCROLL_FLOOR:
				type = IET_SCROLL_FLOOR;
				break;
			case KW_SCROLL_CEILING:
				type = IET_SCROLL_CEILING;
				break;
			case KW_CHANGE_LIGHT:
				type = IET_CHANGE_LIGHT;
				break;
			case KW_CHANGE_WALL_LIGHT:
				type = IET_CHANGE_WALL_LIGHT;
				break;
			default:
				// Invalid type.
				LE_WARNING("Unsupported INF elevator type '%s' (ignored by Dark Forces).", s_infArg1);
				parser.readLine(bufferPos);
				return false;
			}
		}
		Editor_InfElevator* elev = allocElev(item);
		elev->type = type;
		elev->overrideSet = IEO_NONE;
		
		s32 addon = 0;
		bool seqEnd = false;
		while (!seqEnd)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }
			// There is another class in this sequence, exit out.
			if (strstr(line, "CLASS")) { break; }

			char id[256];
			s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArg4, s_infArgExtra);
			KEYWORD action = getKeywordIndex(id);
			if (action == KW_UNKNOWN)
			{
				LE_WARNING("Unknown elevator command - '%s'.", id);
			}
			seqEnd = editor_parseElevatorCommand(argCount, action, seqEnd, elev, addon);
		} // while (!seqEnd)

		return seqEnd;
	}

	bool parseSectorTrigger(TFE_Parser& parser, size_t& bufferPos, s32 argCount, const char* itemName, Editor_InfItem* item)
	{
		// stuff.
		parser.readLine(bufferPos);
		return true;
	}

	bool parseLineTrigger(TFE_Parser& parser, size_t& bufferPos, s32 argCount, const char* itemName, s32 wallNum, Editor_InfItem* item)
	{
		// stuff.
		parser.readLine(bufferPos);
		return true;
	}

	bool parseTeleport(TFE_Parser& parser, size_t& bufferPos, const char* itemName, Editor_InfItem* item)
	{
		// stuff.
		parser.readLine(bufferPos);
		return true;
	}

	bool loadLevelInfFromAsset(Asset* asset)
	{
		char infFile[TFE_MAX_PATH];
		s_fileData.clear();
		if (asset->archive)
		{
			FileUtil::replaceExtension(asset->name.c_str(), "INF", infFile);

			if (asset->archive->openFile(infFile))
			{
				const size_t len = asset->archive->getFileLength();
				s_fileData.resize(len);
				asset->archive->readFile(s_fileData.data(), len);
				asset->archive->closeFile();
			}
		}
		else
		{
			FileUtil::replaceExtension(asset->filePath.c_str(), "INF", infFile);

			FileStream file;
			if (file.open(infFile, Stream::MODE_READ))
			{
				const size_t len = file.getSize();
				s_fileData.resize(len);
				file.readBuffer(s_fileData.data(), (u32)len);
				file.close();
			}
		}

		if (s_fileData.empty()) { return false; }
		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init((char*)s_fileData.data(), s_fileData.size());
		parser.enableBlockComments();
		parser.addCommentString("//");
		parser.convertToUpperCase(true);

		const char* line;
		line = parser.readLine(bufferPos);

		// Keep looping until the version is found.
		while (strncasecmp(line, "INF", 3) != 0 && line)
		{
			line = parser.readLine(bufferPos);
		}
		if (!line)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot find INF version.");
			return false;
		}

		f32 version;
		if (sscanf(line, "INF %f", &version) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot read INF version.");
			return false;
		}
		if (version != 1.0f)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Incorrect INF version %f, should be 1.0.", version);
			return false;
		}

		// Keep looping until ITEMS is found.
		// Note - the editor always ignores the INF limit.
		// TODO: Produce a warning if using the vanilla feature-set.
		s32 itemCount = 0;
		while (1)
		{
			line = parser.readLine(bufferPos);
			if (!line)
			{
				LE_ERROR("Cannot find ITEMS in INF: '%s'.", infFile);
				return false;
			}

			if (sscanf(line, "ITEMS %d", &itemCount) == 1)
			{
				break;
			}
		}
		// Warn about vanilla compatibility.
		if (itemCount > 512)
		{
			LE_WARNING("Too many INF items for vanilla compatibility %d / 512.\nExtra items will be ignored if loading in DOS.", itemCount);
		}

		// Then loop through all of the items and parse their classes.
		s32 wallNum = 0;
		for (s32 i = 0; i < itemCount; i++)
		{
			line = parser.readLine(bufferPos);
			if (!line)
			{
				LE_WARNING("Hit the end of INF '%s' before parsing all items: %d/%d", infFile, i, itemCount);
				return true;
			}

			char item[256], name[256];
			while (sscanf(line, " ITEM: %s NAME: %s NUM: %d", item, name, &wallNum) < 1)
			{
				line = parser.readLine(bufferPos);
				if (!line)
				{
					LE_WARNING("Hit the end of INF '%s' before parsing all items: %d/%d", infFile, i, itemCount);
					return true;
				}
				continue;
			}

			s_levelInf.item.push_back({});
			Editor_InfItem* infItem = &s_levelInf.item.back();
			infItem->name = name;
			infItem->wallNum = wallNum;

			KEYWORD itemType = getKeywordIndex(item);
			switch (itemType)
			{
				case KW_LEVEL:
				{
					line = parser.readLine(bufferPos);
					if (line && strstr(line, "SEQ"))
					{
						while (line = parser.readLine(bufferPos))
						{
							char itemName[256];
							s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", itemName, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArgExtra, s_infArgExtra);
							KEYWORD levelItem = getKeywordIndex(itemName);
							switch (levelItem)
							{
								case KW_SEQEND:
									break;
								case KW_AMB_SOUND:
								{
									//level_addSound(s_infArg0, u32(atof(s_infArg1) * 145.65f), atoi(s_infArg2));
									continue;
								}
							}
							break;
						}
					}
				} break;
				case KW_SECTOR:
				{
					line = parser.readLine(bufferPos);
					if (!line || !strstr(line, "SEQ"))
					{
						continue;
					}

					line = parser.readLine(bufferPos);
					// Loop until seqend since an INF item may have multiple classes.
					while (1)
					{
						if (!line || !strstr(line, "CLASS") || strstr(line, "SEQEND"))
						{
							break;
						}

						char id[256];
						s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArg4, s_infArgExtra);
						KEYWORD itemClass = getKeywordIndex(s_infArg0);
						assert(itemClass != KW_UNKNOWN);

						if (itemClass == KW_ELEVATOR)
						{
							if (parseElevator(parser, bufferPos, name, infItem))
							{
								break;
							}
						}
						else if (itemClass == KW_TRIGGER)
						{
							if (parseSectorTrigger(parser, bufferPos, argCount, name, infItem))
							{
								break;
							}
						}
						else if (itemClass == KW_TELEPORTER)
						{
							if (parseTeleport(parser, bufferPos, name, infItem))
							{
								break;
							}
						}
						else
						{
							// Invalid item class.
							line = parser.readLine(bufferPos);
						}
					}
				} break;
				case KW_LINE:
				{
					line = parser.readLine(bufferPos);
					if (!line || !strstr(line, "SEQ"))
					{
						continue;
					}

					line = parser.readLine(bufferPos);
					// Loop until seqend since an INF item may have multiple classes.
					while (1)
					{
						if (!line || !strstr(line, "CLASS"))
						{
							break;
						}

						char id[256];
						s32 argCount = sscanf(line, " %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
						if (parseLineTrigger(parser, bufferPos, argCount, name, wallNum, infItem))
						{
							break;
						}
					}  // while (!seqEnd) - outer (Line Classes).
				} break;
			}
		}

		return true;
	}

	EditorSector* s_infSector = nullptr;
	Editor_InfItem* s_infItem = nullptr;

	void editor_infSectorEditBegin(const char* sectorName)
	{
		s_infSector = nullptr;
		s_infItem = nullptr;
		if (!sectorName || strlen(sectorName) < 1)
		{
			return;
		}

		const s32 count = (s32)s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (s32 i = 0; i < count; i++, sector++)
		{
			if (sector->name.empty()) { continue; }

			if (strcasecmp(sectorName, sector->name.c_str()) == 0)
			{
				s_infSector = sector;
				break;
			}
		}

		const s32 itemCount = (s32)s_levelInf.item.size();
		Editor_InfItem* item = s_levelInf.item.data();
		for (s32 i = 0; i < itemCount; i++, item++)
		{
			if (strcasecmp(sectorName, item->name.c_str()) == 0)
			{
				s_infItem = item;
				break;
			}
		}
	}

	const char* c_infElevFlagNames[] =
	{
		"Move Floor",         // INF_EFLAG_MOVE_FLOOR = FLAG_BIT(0),	// Move on floor.
		"Move Second Height", //INF_EFLAG_MOVE_SECHT = FLAG_BIT(1),	// Move on second height.
		"Move Ceiling",       //INF_EFLAG_MOVE_CEIL = FLAG_BIT(2),	// Move on ceiling (head has to be close enough?)
	};

	const char* c_infClassName[]=
	{
		"Elevator",
		"Trigger",
		"Teleporter"
	};

	const char* c_infElevVarName[] =
	{
		"Start",		//IEV_START
		"Speed",		//IEV_SPEED
		"Master",		//IEV_MASTER
		"Angle",		//IEV_ANGLE
		"Flags",		//IEV_FLAGS
		"Key",			//IEV_KEY0
		"Key1 (Addon)",	//IEV_KEY1
		"Center",		//IEV_DIR
		"Sound 1",		//IEV_SOUND0
		"Sound 2",		//IEV_SOUND1
		"Sound 3",		//IEV_SOUND2
		"Event_Mask",	//IEV_EVENT_MASK
		"Entity_Mask",	//IEV_ENTITY_MASK
	};

	const char* c_infKeyNames[] =
	{
		"Red",
		"Yellow",
		"Blue"
	};

	const char* c_infEventMaskNames[] =
	{
		"Cross Line Front",	//INF_EVENT_CROSS_LINE_FRONT
		"Cross Line Back",	//INF_EVENT_CROSS_LINE_BACK
		"Enter Sector",		//INF_EVENT_ENTER_SECTOR
		"Leave Sector",		//INF_EVENT_LEAVE_SECTOR
		"Nudge Front",		//INF_EVENT_NUDGE_FRONT
		"Nudge Back",		//INF_EVENT_NUDGE_BACK
		"Explosion",		//INF_EVENT_EXPLOSION
		"Unused",			//INF_EVENT_UNUSED1
		"Shoot Line",		//INF_EVENT_SHOOT_LINE
		"Land",				//INF_EVENT_LAND
	};

	const char* c_infEntityMaskNames[] =
	{
		"Enemy",     // INF_ENTITY_ENEMY,
		"Weapon",    // INF_ENTITY_WEAPON,
		"Smart Obj", // INF_ENTITY_SMART_OBJ,
		"Player",    // INF_ENTITY_PLAYER,
	};

	const u32 c_infEntityMaskFlags[] =
	{
		INF_ENTITY_ENEMY,
		INF_ENTITY_WEAPON,
		INF_ENTITY_SMART_OBJ,
		INF_ENTITY_PLAYER,
	};

	const char* c_infStopDelayTypeName[] =
	{
		"Seconds",		// SDELAY_SECONDS = 0,
		"Hold",			// SDELAY_HOLD,
		"Terminate",	// SDELAY_TERMINATE,
		"Complete",		// SDELAY_COMPLETE,
		"Prev (Buggy)", // SDELAY_PREV_VALUE,
	};

	static s32 s_infClassSelIndex = 0;
	static s32 s_infElevTypeIndex = 0;
	static s32 s_infElevVarIndex = 0;
	static s32 s_infElevVarSelectIndex = -1;
	static s32 s_stopSelected[256] = { 0 };

	#define SELECT(x) s_infElevVarSelectIndex = (s_infElevVarSelectIndex == x) ? -1 : x;

	u32 countBits(u32 bits)
	{
		u32 count = 0;
		while (bits)
		{
			if (bits & 1) { count++; }
			bits >>= 1;
		}
		return count;
	}

	bool editor_infSectorEdit()
	{
		f32 winWidth = 900.0f;
		f32 winHeight = 900.0f;

		pushFont(TFE_Editor::FONT_SMALL);

		bool active = true;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize;
		ImGui::SetNextWindowSize({ winWidth, winHeight });
		if (ImGui::BeginPopupModal("Sector INF", &active, window_flags))
		{
			if (!s_infSector)
			{
				ImGui::TextColored({ 1.0f, 0.2f, 0.2f, 1.0f }, "Sectors with INF functionality must have names.\nPlease name the sector and try again.");
			}
			else if (!s_infItem)
			{
				if (ImGui::Button("Create INF Item"))
				{
					s_levelInf.item.push_back({});
					s_infItem = &s_levelInf.item.back();
					s_infItem->name = s_infSector->name;
					s_infItem->wallNum = -1;
				}
			}
			else if (s_infItem)
			{
				// Display it for now.
				ImGui::Text("Item: %s, Sector ID: %d", s_infItem->name.c_str(), s_infSector->id);
				ImGui::Separator();

				if (ImGui::Button("+Add Class"))
				{
					if (s_infClassSelIndex == IIC_ELEVATOR)
					{
						Editor_InfElevator* newElev = allocElev(s_infItem);
						if (newElev)
						{
							newElev->type = Editor_InfElevType(s_infElevTypeIndex);
						}
					}
					else if (s_infClassSelIndex == IIC_TRIGGER)
					{
						// TODO
					}
					else if (s_infClassSelIndex == IIC_TELEPORTER)
					{
						// TODO
					}
				}
				ImGui::SameLine(0.0f, 32.0f);
				ImGui::SetNextItemWidth(128.0f);
				if (ImGui::BeginCombo("##SectorClassCombo", c_infClassName[s_infClassSelIndex]))
				{
					s32 count = (s32)TFE_ARRAYSIZE(c_infClassName);
					for (s32 i = 0; i < count; i++)
					{
						if (ImGui::Selectable(c_infClassName[i], i == s_infClassSelIndex))
						{
							s_infClassSelIndex = i;
						}
						//setTooltip(c_infClassName[i].tooltip.c_str());
					}
					ImGui::EndCombo();
				}
				if (s_infClassSelIndex == IIC_ELEVATOR)
				{
					ImGui::SameLine(0.0f, 8.0f);
					ImGui::SetNextItemWidth(160.0f);

					if (ImGui::BeginCombo("##SectorTypeCombo", c_infElevTypeName[s_infElevTypeIndex]))
					{
						s32 count = (s32)TFE_ARRAYSIZE(c_infElevTypeName);
						for (s32 i = 0; i < count; i++)
						{
							if (ImGui::Selectable(c_infElevTypeName[i], i == s_infElevTypeIndex))
							{
								s_infElevTypeIndex = i;
							}
							//setTooltip(c_infClassName[i].tooltip.c_str());
						}
						ImGui::EndCombo();
					}
				}
				ImGui::Separator();

				if (ImGui::BeginChild(0x4040404, { 0, 0 }, false))
				{
					s32 deleteIndex = -1;

					const s32 count = s_infItem->classData.size();
					Editor_InfClass** dataList = s_infItem->classData.data();
					char buffer[256];
					for (s32 i = 0; i < count; i++)
					{
						if (ImGui::BeginChild(0x303 + i, { 0, 600 }, true))
						{
							Editor_InfClass* data = dataList[i];
							switch (data->classId)
							{
								case IIC_ELEVATOR:
								{
									Editor_InfElevator* elev = getElevFromClassData(data);
									assert(elev);

									ImGui::TextColored({ 0.2f, 1.0f, 0.2f, 1.0f }, "Class: Elevator %s", c_infElevTypeName[elev->type]);
									ImGui::SameLine(0.0f, 16.0f);
									if (ImGui::Button("-Delete"))
									{
										deleteIndex = i;
										break;
									}

									sprintf(buffer, "##VariableList%d", i);
									ImGui::Text("%s", "Variables");
									if (elev->overrideSet & IEO_VAR_MASK)
									{
										f32 varHeight = 26.0f * countBits(elev->overrideSet & IEO_VAR_MASK) + 16;
										// Expand if flags are selected.
										if (s_infElevVarSelectIndex == IEV_FLAGS || s_infElevVarSelectIndex == IEV_ENTITY_MASK) { varHeight += 26.0f; }
										else if (s_infElevVarSelectIndex == IEV_EVENT_MASK) { varHeight += 26.0f * 3.0f; }

										ImGui::BeginChild(buffer, { 0.0f, varHeight }, true);
										{
											const u32 overrides = elev->overrideSet;
											if (overrides & IEO_START)
											{
												bool sel = s_infElevVarSelectIndex == IEV_START;
												if (ImGui::Selectable("Start:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_START);
												}
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::SetNextItemWidth(128.0f);
												ImGui::InputInt("##Start", &elev->start);
											}
											if (overrides & IEO_SPEED)
											{
												bool sel = s_infElevVarSelectIndex == IEV_SPEED;
												if (ImGui::Selectable("Speed:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_SPEED);
												}
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::SetNextItemWidth(128.0f);
												ImGui::InputFloat("##Speed", &elev->speed, 0.1f, 1.0f, 3);
											}
											if (overrides & IEO_MASTER)
											{
												bool sel = s_infElevVarSelectIndex == IEV_MASTER;
												if (ImGui::Selectable("Master:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_MASTER);
												}
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::Checkbox("##Master", &elev->master);
											}
											if (overrides & IEO_ANGLE)
											{
												bool sel = s_infElevVarSelectIndex == IEV_ANGLE;
												if (ImGui::Selectable("Angle:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_ANGLE);
												}
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::SetNextItemWidth(128.0f);
												ImGui::SliderAngle("##AngleSlider", &elev->angle);
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::SetNextItemWidth(128.0f);
												f32 angle = elev->angle * 180.0f / PI;
												if (ImGui::InputFloat("##Angle", &angle, 0.1f, 1.0f, 3))
												{
													elev->angle = angle * PI / 180.0f;
												}
											}
											if (overrides & IEO_FLAGS)
											{
												bool sel = s_infElevVarSelectIndex == IEV_FLAGS;
												if (ImGui::Selectable("Flags:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_FLAGS);
												}
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::SetNextItemWidth(128.0f);
												ImGui::InputUInt("##FlagsTextInput", &elev->flags);
												if (s_infElevVarSelectIndex == IEV_FLAGS)
												{
													for (s32 i = 0; i < TFE_ARRAYSIZE(c_infElevFlagNames); i++)
													{
														if (i != 0) { ImGui::SameLine(0.0f, 8.0f); }
														ImGui::CheckboxFlags(c_infElevFlagNames[i], &elev->flags, 1 << i);
													}
												}
											}
											if (overrides & IEO_KEY0)
											{
												bool sel = s_infElevVarSelectIndex == IEV_KEY0;
												if (ImGui::Selectable("Key:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_KEY0);
												}
												ImGui::SameLine(0.0f, 8.0f);
												s32 keyIndex = elev->key[0] - KeyItem::KEY_RED;
												sprintf(buffer, "###KeyVar%d", i);
												ImGui::SetNextItemWidth(128.0f);
												if (ImGui::Combo(buffer, &keyIndex, c_infKeyNames, TFE_ARRAYSIZE(c_infKeyNames)))
												{
													elev->key[0] = KeyItem(keyIndex + KeyItem::KEY_RED);
												}

											}
											if (overrides & IEO_KEY1)
											{
												bool sel = s_infElevVarSelectIndex == IEV_KEY1;
												if (ImGui::Selectable("Key 1 (Addon):", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_KEY1);
												}
												ImGui::SameLine(0.0f, 8.0f);
												s32 keyIndex = elev->key[1] == 0 ? 0 : elev->key[1] - KeyItem::KEY_RED + 1;
												sprintf(buffer, "###KeyVar_1_%d", i);
												ImGui::SetNextItemWidth(128.0f);
												if (ImGui::Combo(buffer, &keyIndex, c_infKeyNames, TFE_ARRAYSIZE(c_infKeyNames)))
												{
													if (keyIndex == 0) { elev->key[1] = KeyItem::KEY_NONE; }
													else
													{
														elev->key[1] = KeyItem(keyIndex + KeyItem::KEY_RED - 1);
													}
												}
											}
											if (overrides & IEO_DIR)
											{
												bool sel = s_infElevVarSelectIndex == IEV_DIR;
												if (ImGui::Selectable("Center:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_DIR);
												}
												ImGui::SameLine(0.0f, 8.0f);
												sprintf(buffer, "###DirVar%d", i);
												ImGui::SetNextItemWidth(160.0f);
												ImGui::InputFloat2(buffer, elev->dirOrCenter.m, 3);
											}
											if (overrides & IEO_SOUND0)
											{
												bool sel = s_infElevVarSelectIndex == IEV_SOUND0;
												if (ImGui::Selectable("Sound 1:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_SOUND0);
												}
												ImGui::SameLine(0.0f, 8.0f);
												char soundBuffer[256];
												strcpy(soundBuffer, elev->sounds[0].c_str());

												sprintf(buffer, "###Sound1_%d", i);
												ImGui::SetNextItemWidth(160.0f);
												if (ImGui::InputText(buffer, soundBuffer, 256))
												{
													elev->sounds[0] = soundBuffer;
												}
											}
											if (overrides & IEO_SOUND1)
											{
												bool sel = s_infElevVarSelectIndex == IEV_SOUND1;
												if (ImGui::Selectable("Sound 2:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_SOUND1);
												}
												ImGui::SameLine(0.0f, 8.0f);
												char soundBuffer[256];
												strcpy(soundBuffer, elev->sounds[1].c_str());

												sprintf(buffer, "###Sound2_%d", i);
												ImGui::SetNextItemWidth(160.0f);
												if (ImGui::InputText(buffer, soundBuffer, 256))
												{
													elev->sounds[1] = soundBuffer;
												}
											}
											if (overrides & IEO_SOUND2)
											{
												bool sel = s_infElevVarSelectIndex == IEV_SOUND2;
												if (ImGui::Selectable("Sound 3:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_SOUND2);
												}
												ImGui::SameLine(0.0f, 8.0f);
												char soundBuffer[256];
												strcpy(soundBuffer, elev->sounds[2].c_str());

												sprintf(buffer, "###Sound3_%d", i);
												ImGui::SetNextItemWidth(160.0f);
												if (ImGui::InputText(buffer, soundBuffer, 256))
												{
													elev->sounds[2] = soundBuffer;
												}
											}
											if (overrides & IEO_EVENT_MASK)
											{
												bool sel = s_infElevVarSelectIndex == IEV_EVENT_MASK;
												if (ImGui::Selectable("Event_Mask:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_EVENT_MASK);
												}
												ImGui::SameLine(0.0f, 8.0f);

												ImGui::SetNextItemWidth(128.0f);
												ImGui::InputInt("##EventMaskTextInput", &elev->eventMask);

												if (s_infElevVarSelectIndex == IEV_EVENT_MASK)
												{
													for (s32 i = 0; i < TFE_ARRAYSIZE(c_infEventMaskNames); i++)
													{
														if ((i % 4) != 0) { ImGui::SameLine(200.0f * (i % 4), 0.0f); }
														ImGui::CheckboxFlags(c_infEventMaskNames[i], (u32*)&elev->eventMask, 1 << i);
													}
												}
											}
											if (overrides & IEO_ENTITY_MASK)
											{
												bool sel = s_infElevVarSelectIndex == IEV_ENTITY_MASK;
												if (ImGui::Selectable("Entity_Mask:", &sel, 0, { 100.0f, 0.0f }))
												{
													SELECT(IEV_ENTITY_MASK);
												}
												ImGui::SameLine(0.0f, 8.0f);

												ImGui::SetNextItemWidth(128.0f);
												ImGui::InputInt("##EntityMaskTextInput", &elev->entityMask);

												if (s_infElevVarSelectIndex == IEV_ENTITY_MASK)
												{
													for (s32 i = 0; i < TFE_ARRAYSIZE(c_infEntityMaskNames); i++)
													{
														if ((i % 4) != 0) { ImGui::SameLine(200.0f * (i % 4), 0.0f); }
														ImGui::CheckboxFlags(c_infEntityMaskNames[i], (u32*)&elev->entityMask, c_infEntityMaskFlags[i]);
													}
												}
											}
										}
										ImGui::EndChild();
									}

									if (ImGui::Button("+Add Variable"))
									{
										if (s_infElevVarIndex >= 0)
										{
											elev->overrideSet |= (1 << s_infElevVarIndex);
										}
									}

									ImGui::SameLine(0.0f, 16.0f);
									ImGui::SetNextItemWidth(128.0f);
									if (ImGui::BeginCombo("##ElevatorVariableCombo", c_infElevVarName[s_infElevVarIndex]))
									{
										s32 count = (s32)TFE_ARRAYSIZE(c_infElevVarName);
										for (s32 i = 0; i < count; i++)
										{
											if (ImGui::Selectable(c_infElevVarName[i], i == s_infElevVarIndex))
											{
												s_infElevVarIndex = i;
											}
											//setTooltip(c_infClassName[i].tooltip.c_str());
										}
										ImGui::EndCombo();
									}

									ImGui::SameLine(0.0f, 32.0f);
									if (ImGui::Button("-Remove Variable"))
									{
										if (s_infElevVarSelectIndex >= 0)
										{
											elev->overrideSet &= ~(1 << s_infElevVarSelectIndex);
										}
										s_infElevVarSelectIndex = -1;
									}

									if (elev->overrideSet & IEO_SLAVES)
									{
									}

									ImGui::Separator();

									if (elev->overrideSet & IEO_STOPS)
									{
										const s32 stopCount = (s32)elev->stops.size();
										ImGui::Text("Stops: %d", stopCount);
										sprintf(buffer, "##Stops%d", i);
										ImGui::BeginChild(buffer, { 0.0f, 140.0f }, true);
										{
											Editor_InfStop* stop = elev->stops.data();
											for (s32 s = 0; s < stopCount; s++, stop++)
											{
												sprintf(buffer, "Stop: %d##List%d", s, i);
												if (ImGui::Selectable(buffer, s == s_stopSelected[i], 0, { 80.0f, 0.0f }))
												{
													s_stopSelected[i] = s;
												}
												ImGui::SameLine(0.0f, 16.0f);
												ImGui::Text("Value:");
												ImGui::SameLine(0.0f, 8.0f);

												sprintf(buffer, "###StopValue%d_%d", i, s);
												ImGui::SetNextItemWidth(128.0f);
												ImGui::InputFloat(buffer, &stop->value, 0.1f, 1.0f, 3);
												ImGui::SameLine(0.0f, 16.0f);

												sprintf(buffer, "###StopValueRel%d_%d", i, s);
												ImGui::Text("Relative:");
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::Checkbox(buffer, &stop->relative);
												ImGui::SameLine(0.0f, 16.0f);

												ImGui::Text("Delay Type:");
												ImGui::SameLine(0.0f, 8.0f);
												ImGui::SetNextItemWidth(100.0f);
												sprintf(buffer, "###StopDelayType%d_%d", i, s);
												if (ImGui::BeginCombo(buffer, c_infStopDelayTypeName[stop->delayType]))
												{
													s32 count = (s32)TFE_ARRAYSIZE(c_infStopDelayTypeName);
													for (s32 c = 0; c < count; c++)
													{
														if (ImGui::Selectable(c_infStopDelayTypeName[c], c == stop->delayType))
														{
															stop->delayType = Editor_InfStopDelayType(c);
														}
														//setTooltip(c_infClassName[i].tooltip.c_str());
													}
													ImGui::EndCombo();
												}
												ImGui::SameLine(0.0f, 16.0f);

												ImGui::Text("Delay:");
												ImGui::SameLine(0.0f, 8.0f);
												sprintf(buffer, "###StopDelay%d_%d", i, s);
												ImGui::SetNextItemWidth(128.0f);
												ImGui::InputFloat(buffer, &stop->delay, 0.1f, 1.0f, 3);


											}
										}
										ImGui::EndChild();
									}

								} break;
								case IIC_TRIGGER:
								{
									ImGui::TextColored({ 0.2f, 1.0f, 0.2f, 1.0f }, "Class: Trigger");
								} break;
								case IIC_TELEPORTER:
								{
									ImGui::TextColored({ 0.2f, 1.0f, 0.2f, 1.0f }, "Class: Teleporter");
								} break;
							}
						}
						ImGui::EndChild();
					}
					ImGui::EndChild();

					if (deleteIndex >= 0 && deleteIndex < count)
					{
						switch (s_infItem->classData[deleteIndex]->classId)
						{
							case IIC_ELEVATOR:
							{
								Editor_InfElevator* elev = getElevFromClassData(s_infItem->classData[deleteIndex]);
								assert(elev);
								freeElevator(elev);
							} break;
							case IIC_TRIGGER:
							{
								Editor_InfTrigger* trigger = getTriggerFromClassData(s_infItem->classData[deleteIndex]);
								assert(trigger);
								freeTrigger(trigger);
							} break;
							case IIC_TELEPORTER:
							{
								Editor_InfTeleporter* teleporter = getTeleportFromClassData(s_infItem->classData[deleteIndex]);
								assert(teleporter);
								freeTeleporter(teleporter);
							} break;
						}

						for (s32 i = deleteIndex; i < count - 1; i++)
						{
							s_infItem->classData[i] = s_infItem->classData[i + 1];
						}
						s_infItem->classData.pop_back();
					}
				}
			}
			else
			{
				// Create new.
				ImGui::Text("Stub... create new INF item.");
			}

			ImGui::EndPopup();
		}

		popFont();

		return !active;
	}
}
