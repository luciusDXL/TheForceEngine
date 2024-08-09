#include "ls_system.h"
#include "levelEditorScripts.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_ForceScript/forceScript.h>
#include <TFE_ForceScript/float2.h>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>

namespace LevelEditor
{
	namespace  // Static helper functions.
	{
		std::string formatValue(s32 typeId, void* ref)
		{
			std::string value;
			char tmp[256];

			if (typeId == TFE_ForceScript::getObjectTypeId(TFE_ForceScript::FSTYPE_STRING))
			{
				value = *(std::string*)ref;
			}
			else if (typeId == TFE_ForceScript::getObjectTypeId(TFE_ForceScript::FSTYPE_FLOAT2))
			{
				value = toString(*(TFE_ForceScript::float2*)ref);
			}
			else
			{
				switch (typeId)
				{
					case 2:
					{
						char local = *(char*)(ref);
						sprintf(tmp, "%c", local);
						value = tmp;
						break;
					}
					case 3:
					{
						s16 local = *(s16*)(ref);
						sprintf(tmp, "%d", local);
						value = tmp;
						break;
					}
					case 4:
					{
						s32 local = *(s32*)(ref);
						sprintf(tmp, "%d", local);
						value = tmp;
						break;
					}
					case 5:
					{
						s64 local = *(s64*)(ref);
						sprintf(tmp, "%lld", local);
						value = tmp;
						break;
					}
					case 6:
					{
						u8 local = *(u8*)(ref);
						sprintf(tmp, "%u", local);
						value = tmp;
						break;
					}
					case 7:
					{
						u16 local = *(u16*)(ref);
						sprintf(tmp, "%u", local);
						value = tmp;
						break;
					}
					case 8:
					{
						u32 local = *(u32*)(ref);
						sprintf(tmp, "%u", local);
						value = tmp;
						break;
					}
					case 9:
					{
						u64 local = *(u64*)(ref);
						sprintf(tmp, "%llu", local);
						value = tmp;
						break;
					}
					case 10:
					{
						f32 local = *(f32*)(ref);
						sprintf(tmp, "%g", local);
						value = tmp;
						break;
					}
					case 11:
					{
						f64 local = *(f64*)(ref);
						sprintf(tmp, "%g", local);
						value = tmp;
						break;
					}
					default:
					{
						value = "?";
					}
				}
			}
			return value;
		}

		void outputPrintFormat(LeMsgType msgType, asIScriptGeneric *gen)
		{
			const s32 argCount = gen->GetArgCount();

			void* ref = gen->GetArgAddress(0);
			s32 typeId = gen->GetArgTypeId(0);
			std::string fmt = *((std::string*)ref);
			std::string output = fmt;

			// parse the string for {} pairs.
			const char* key = "{}";
			const size_t keyLen = strlen(key);
			size_t len = output.length();

			s32 index = 1;
			for (size_t pos = 0; pos < len && index < argCount;)
			{
				pos = output.find(key, pos);
				if (pos == std::string::npos)
				{
					break;
				}

				ref = gen->GetArgAddress(index);
				typeId = gen->GetArgTypeId(index);
				index++;

				output.replace(pos, keyLen, formatValue(typeId, ref));
				len = output.length();
				pos += 2;
			}
			infoPanelAddMsg(msgType, "%s", output.c_str());
		}
	}

	void LS_System::error(asIScriptGeneric *gen)
	{
		outputPrintFormat(LE_MSG_ERROR, gen);
	}

	void LS_System::warning(asIScriptGeneric *gen)
	{
		outputPrintFormat(LE_MSG_WARNING, gen);
	}
	
	void LS_System::print(asIScriptGeneric *gen)
	{
		outputPrintFormat(LE_MSG_INFO, gen);
	}

	void LS_System::clearOutput()
	{
		infoPanelClearMessages();
	}

	void LS_System::runScript(std::string& scriptName)
	{
		runLevelScript(scriptName.c_str());
	}

	void LS_System::showScript(std::string& scriptName)
	{
		showLevelScript(scriptName.c_str());
	}

	bool LS_System::scriptRegister(asIScriptEngine* engine)
	{
		s32 res = 0;
		// Object Type
		res = engine->RegisterObjectType("System", sizeof(LS_System), asOBJ_VALUE | asOBJ_POD); assert(res >= 0);
		// Properties
		res = engine->RegisterObjectProperty("System", "int version", asOFFSET(LS_System, version));  assert(res >= 0);

		//------------------------------------
		// Functions
		//------------------------------------
		// Currently support up to 8 arguments + format string.
		// This has to be a static member of the class since it use the GENERIC calling convention, but it is still added as an object method
		// so the script API follows the desired `system.func()` pattern.
		res = engine->RegisterObjectMethod("System",
			"void error(const string &in, ?&in arg0 = 0, ?&in arg1 = 0, ?&in arg2 = 0, ?&in arg3 = 0, ?&in arg4 = 0, ?&in arg5 = 0, ?&in arg6 = 0, ?&in arg7 = 0)",
			asFUNCTION(error), asCALL_GENERIC); assert(res >= 0);
		res = engine->RegisterObjectMethod("System",
			"void warning(const string &in, ?&in arg0 = 0, ?&in arg1 = 0, ?&in arg2 = 0, ?&in arg3 = 0, ?&in arg4 = 0, ?&in arg5 = 0, ?&in arg6 = 0, ?&in arg7 = 0)",
			asFUNCTION(warning), asCALL_GENERIC); assert(res >= 0);
		res = engine->RegisterObjectMethod("System",
			"void print(const string &in, ?&in arg0 = 0, ?&in arg1 = 0, ?&in arg2 = 0, ?&in arg3 = 0, ?&in arg4 = 0, ?&in arg5 = 0, ?&in arg6 = 0, ?&in arg7 = 0)",
			asFUNCTION(print), asCALL_GENERIC); assert(res >= 0);

		res = engine->RegisterObjectMethod("System", "void clearOutput()", asMETHOD(LS_System, clearOutput), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("System", "void runScript(const string &in)", asMETHOD(LS_System, runScript), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("System", "void showScript(const string &in)", asMETHOD(LS_System, showScript), asCALL_THISCALL);  assert(res >= 0);

		// Script variable.
		res = engine->RegisterGlobalProperty("System system", this);  assert(res >= 0);
		return res >= 0;
	}
}

#endif