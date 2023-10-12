#include "errorMessages.h"
#include <assert.h>

namespace TFE_Editor
{
	static const char* c_errorMsg[] =
	{
		// ERROR_INVALID_EXPORT_PATH
		"Export Path '%s' is invalid, cannot export assets!\n"
		"Please go to the 'Editor' menu, select 'Editor Config',\n"
		"and setup a valid 'Export Path'.",
	};

	const char* getErrorMsg(EditorError err)
	{
		assert(err >= 0 && err < ERROR_COUNT);
		return c_errorMsg[err];
	}
}
