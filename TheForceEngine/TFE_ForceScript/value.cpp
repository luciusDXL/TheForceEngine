#include "value.h"

#ifdef VM_ENABLE
namespace TFE_ForceScript
{
	void fsValue_toString(FsValue value, char* outStr)
	{
		FS_Type type = FS_Type(fsValue_getValueData(value) & 15ull);
		switch (type)
		{
			case FST_INT:
			{
				sprintf(outStr, "%d", *fsValue_getIntPtr(value));
			} break;
			case FST_FLOAT:
			{
				sprintf(outStr, "%f", *fsValue_getFloatPtr(value));
			} break;
		}
	}
}
#endif