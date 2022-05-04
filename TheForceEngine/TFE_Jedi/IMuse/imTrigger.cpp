#include "imuse.h"
#include "imSoundFader.h"
#include <TFE_System/system.h>
#include <TFE_Audio/midi.h>
#include <assert.h>

namespace TFE_Jedi
{
	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct ImTrigger
	{
		ImSoundId soundId;
		s32 marker;
		ptrdiff_t opcode;			// opcode is sometimes used to pass pointers.
		s32 args[10];
	};

	struct ImDeferCmd
	{
		s32 time;
		ptrdiff_t opcode;
		s32 args[10];
	};

	typedef void(*ImTriggerCmdFunc)(char*);

	enum ImTriggerConst
	{
		ImMaxDeferredCmd = 8,
	};

	/////////////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////////////
	static ImTrigger s_triggers[imChannelCount];
	static ImDeferCmd s_deferCmd[ImMaxDeferredCmd];
	static s32 s_imDeferredCmds = 0;

	void ImHandleDeferredCommand(ImDeferCmd* cmd);
	void ImTriggerExecute(ImTrigger* trigger, void* marker);

	/////////////////////////////////////////////////////////// 
	// API
	/////////////////////////////////////////////////////////// 
	s32 ImSetTrigger(ImSoundId soundId, s32 marker, ptrdiff_t opcode)
	{
		if (!soundId) { return imArgErr; }

		ImTrigger* trigger = s_triggers;
		for (s32 i = 0; i < imChannelCount; i++, trigger++)
		{
			if (!trigger->soundId)
			{
				trigger->soundId = soundId;
				trigger->marker = marker;
				trigger->opcode = opcode;

				// The code beyond this point handles variable parameters - but this isn't used in Dark Forces.
				// It blindly copies 10 arguments from the stack and stores them in trigger->args[]
				return imSuccess;
			}
		}
		IM_LOG_WRN("tr unable to alloc trigger.");
		return imAllocErr;
	}

	// Returns the number of matching triggers.
	// '-1' acts as a wild card.
	s32 ImCheckTrigger(ImSoundId soundId, s32 marker, ptrdiff_t opcode)
	{
		ImTrigger* trigger = s_triggers;
		s32 count = 0;
		for (s32 i = 0; i < imChannelCount; i++, trigger++)
		{
			if (trigger->soundId)
			{
				if ((soundId == imSoundWildcard || soundId == trigger->soundId) &&
					(marker  == -1 || marker  == trigger->marker) &&
					(opcode  == -1 || opcode  == trigger->opcode))
				{
					count++;
				}
			}
		}
		return count;
	}

	s32 ImClearTrigger(ImSoundId soundId, s32 marker, ptrdiff_t opcode)
	{
		ImTrigger* trigger = s_triggers;
		for (s32 i = 0; i < imChannelCount; i++, trigger++)
		{
			// Only clear set triggers and match Sound ID
			if (trigger->soundId && (soundId == imSoundWildcard || soundId == trigger->soundId))
			{
				// Match marker and opcode.
				if ((marker == -1 || marker == trigger->marker) && (opcode == -1 || opcode == trigger->opcode))
				{
					trigger->soundId = IM_NULL_SOUNDID;
				}
			}
		}
		return imSuccess;
	}

	s32 ImClearTriggersAndCmds()
	{
		for (s32 i = 0; i < imChannelCount; i++)
		{
			s_triggers[i].soundId = IM_NULL_SOUNDID;
		}
		for (s32 i = 0; i < ImMaxDeferredCmd; i++)
		{
			s_deferCmd[i].time = 0;
		}
		s_imDeferredCmds = 0;
		return imSuccess;
	}

	// The original function can take a variable number of arguments, but it is used exactly once in Dark Forces,
	// so I simplified it.
	s32 ImDeferCommand(s32 time, ptrdiff_t opcode, s32 arg1)
	{
		ImDeferCmd* cmd = s_deferCmd;
		if (time == 0)
		{
			return imArgErr;
		}

		for (s32 i = 0; i < ImMaxDeferredCmd; i++, cmd++)
		{
			if (cmd->time == 0)
			{
				cmd->opcode = opcode;
				cmd->time = time;

				// This copies variable arguments into cmd->args[], for Dark Forces, only a single argument is used.
				cmd->args[0] = arg1;
				s_imDeferredCmds = 1;
				return imSuccess;
			}
		}

		IM_LOG_WRN("tr unable to alloc deferred cmd.");
		return imAllocErr;
	}
		
	void ImHandleDeferredCommands()
	{
		ImDeferCmd* cmd = s_deferCmd;
		if (s_imDeferredCmds)
		{
			s_imDeferredCmds = 0;
			for (s32 i = 0; i < ImMaxDeferredCmd; i++, cmd++)
			{
				if (cmd->time)
				{
					s_imDeferredCmds = 1;
					cmd->time--;
					if (cmd->time == 0)
					{
						ImHandleDeferredCommand(cmd);
					}
				}
			}
		}
	}

	void ImSetSoundTrigger(u32 soundId, void* marker)
	{
		ImTrigger* trigger = s_triggers;
		// Look for the matching trigger.
		for (s32 i = 0; i < imChannelCount; i++, trigger++)
		{
			s32 triggerSndId = trigger->soundId;
			// Trigger is null or doesn't match.
			if (!triggerSndId || triggerSndId != soundId)
			{
				continue;
			}

			// The code never reaches this point in Dark Forces.
			if (trigger->marker)
			{
				IM_LOG_ERR("trigger->marker should always be 0 in Dark Forces.");
				assert(0);
			#if 0
				if ((marker >= 0x80 && trigger->marker == 0x80) || (marker <= 0x80 && trigger->marker == marker))
				{
					//
				}
			#endif
			}
			ImTriggerExecute(trigger, marker);
		}
	}

	////////////////////////////////////
	// Internal
	////////////////////////////////////
	void ImHandleDeferredCommand(ImDeferCmd* cmd)
	{
		// In Dark Forces, only stop sound is used, basically to deal stopping a sound after an iMuse transition.
		// In the original code, this calls the dispatch function and pushes all 10 arguments.
		// A non-dispatch version can be created here with a simple case statement.
		if (cmd->opcode == imStopSound)
		{
			ImStopSound(cmd->args[0]);
		}
		else
		{
			IM_LOG_ERR("Unimplemented deferred command.");
			assert(0);
		}
	}

	void ImTriggerExecute(ImTrigger* trigger, void* marker)
	{
		trigger->soundId = 0;
		if (trigger->opcode < imUndefined)
		{
			// In the original code, this passed in all 10 parameters.
			// ImProcessCommand(trigger->opcode, *arg0, *arg1, *arg2);
			IM_LOG_ERR("Unimplemented trigger opcode.");
			assert(0);
		}
		else
		{
			ImTriggerCmdFunc(trigger->opcode)((char*)marker);
		}
	}

}  // namespace TFE_Jedi
