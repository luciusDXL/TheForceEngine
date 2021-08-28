#include "updateLogic.h"
#include "time.h"
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/InfSystem/message.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static Task* s_logicUpdateTask = nullptr;
	static Allocator* s_logicUpdateList = nullptr;

	void updateLogic_clearTask()
	{
		s_logicUpdateList = nullptr;
		s_logicUpdateTask = nullptr;
	}
}  // namespace TFE_DarkForces