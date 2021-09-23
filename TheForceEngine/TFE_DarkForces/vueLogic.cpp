#include "vueLogic.h"
#include "time.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	enum VueFlags
	{
		VUE_PAUSED = FLAG_BIT(0),
	};

	enum VueFrameFlags
	{
		VFRAME_FIRST = FLAG_BIT(0),
	};

	struct VueLogic
	{
		Logic logic;

		Allocator* frames;
		Task* task;
		s32 isCamera;
		u32 frameDelay;
		s32 u28;
		s32 u2c;
		u32 flags;
	};

	struct VueFrame
	{
		fixed16_16 mtx[9];
		vec3_fixed offset;

		angle14_16 maxYaw;
		angle14_16 maxPitch;
		s32 pad;
		u32 flags;
	};

	static char* s_workBuffer = nullptr;
	static size_t s_workBufferSize = 0;

	JBool vueLogicSetupFunc(Logic* logic, KEYWORD key);
	void vueLogicTaskFunc(MessageType msg);
	void vueLogicCleanupFunc(Logic *logic);

	Logic* obj_createVueLogic(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		VueLogic* vueLogic = (VueLogic*)level_alloc(sizeof(VueLogic));

		vueLogic->logic.obj = obj;
		vueLogic->frames = nullptr;
		vueLogic->frameDelay = 9;	// 9 Ticks between frames = ~16 fps
		vueLogic->u28 = 0;
		vueLogic->flags &= ~VUE_PAUSED;

		obj->entityFlags |= ETFLAG_CAN_WAKE;

		Task* task = createTask("vueLogic", vueLogicTaskFunc);
		vueLogic->task = task;
		task_setUserData(task, vueLogic);

		obj_addLogic(obj, (Logic*)vueLogic, task, vueLogicCleanupFunc);
		*setupFunc = vueLogicSetupFunc;

		return (Logic*)vueLogic;
	}

	void loadVueFile(Allocator* vueList, char* transformName, TFE_Parser* parser)
	{
		if (!strcasecmp(transformName, "camera"))
		{
			// TODO(Core Game Loop Release)
		}

		// Matrix 0
		fixed16_16 mtx0[9];
		mtx0[0] = ONE_16;
		mtx0[1] = 0;
		mtx0[2] = 0;

		mtx0[3] = 0;
		mtx0[4] = 1;
		mtx0[5] = ONE_16 - 1;

		mtx0[6] = 0;
		mtx0[7] = -ONE_16 + 1;
		mtx0[8] = 1;

		// Matrix 1
		fixed16_16 mtx1[9];
		mtx1[0] = ONE_16;
		mtx1[1] = 0;
		mtx1[2] = 0;

		mtx1[3] = 0;
		mtx1[4] = 1;
		mtx1[5] = ONE_16 - 1;

		mtx1[6] = 0;
		mtx1[7] = -ONE_16 + 1;
		mtx1[8] = 1;

		VueFrame* frame = (VueFrame*)allocator_newItem(vueList);
		frame->flags |= VFRAME_FIRST;

		size_t bufferPos = 0;
		while (1)
		{
			const char* line = parser->readLine(bufferPos);
			if (!line)
			{
				break;
			}

			char name[32];
			f32 f00, f01, f02, f03, f04, f05, f06, f07, f08, f09, f10, f11;
			s32 count = sscanf(line, "transform %s %f %f %f %f %f %f %f %f %f %f %f %f", name, &f00, &f01, &f02, &f03, &f04, &f05, &f06, &f07, &f08, &f09, &f10, &f11);
			if (count == 13)
			{
				// Is this the correct transform?
				if (transformName[0] != '*' && strcasecmp(name, transformName))
				{
					continue;
				}
				frame = (VueFrame*)allocator_newItem(vueList);

				// Rotation/Scale matrix.
				fixed16_16 frameMtx[9];
				frameMtx[0] = floatToFixed16(f00);
				frameMtx[1] = floatToFixed16(f01);
				frameMtx[2] = floatToFixed16(f02);
				frameMtx[3] = floatToFixed16(f03);
				frameMtx[4] = floatToFixed16(f04);
				frameMtx[5] = floatToFixed16(f05);
				frameMtx[6] = floatToFixed16(f06);
				frameMtx[7] = floatToFixed16(f07);
				frameMtx[8] = floatToFixed16(f08);

				// Transform to DF coordinate system.
				fixed16_16 tempMtx[9];
				mulMatrix3x3(mtx1, frameMtx, tempMtx);
				mulMatrix3x3(tempMtx, mtx0, frame->mtx);

				frame->offset.x =  floatToFixed16(f09);
				frame->offset.y = -floatToFixed16(f11);
				frame->offset.z =  floatToFixed16(f10);

				frame->maxPitch = 8191;
				frame->flags &= ~VFRAME_FIRST;
			}
		}
	}
		
	char* allocateWorkBuffer(size_t size)
	{
		if (size > s_workBufferSize)
		{
			s_workBufferSize = size + 1024;
			s_workBuffer = (char*)game_realloc(s_workBuffer, s_workBufferSize);
		}
		assert(s_workBuffer);
		return s_workBuffer;
	}

	JBool createParserFromFile(FilePath* filePath, TFE_Parser* parser)
	{
		assert(filePath && parser);

		FileStream file;
		size_t size = 0;
		char* buffer = nullptr;
		if (file.open(filePath, FileStream::MODE_READ))
		{
			size = file.getSize();
			buffer = allocateWorkBuffer(size);
			file.readBuffer(buffer, (u32)size);
		}
		file.close();

		if (!buffer || !size)
		{
			return JFALSE;
		}

		parser->init(buffer, size);
		parser->addCommentString("//");
		parser->addCommentString("#");
		return JTRUE;
	}

	Allocator* key_loadVue(char* arg1, char* arg2, s32 isCamera)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(arg1, &filePath))
		{
			TFE_System::logWrite(LOG_ERROR, "VUE", "key_loadVue: COULD NOT OPEN.");
			return nullptr;
		}
		TFE_Parser parser;
		if (!createParserFromFile(&filePath, &parser))
		{
			TFE_System::logWrite(LOG_ERROR, "VUE", "key_loadVue: COULD NOT OPEN.");
			return nullptr;
		}
		
		Allocator* vueList = allocator_create(sizeof(VueFrame));
		loadVueFile(vueList, arg2, &parser);
		
		return vueList;
	}

	Allocator* key_appendVue(char* arg1, char* arg2, Allocator* frames)
	{
		FilePath filePath;
		if (!TFE_Paths::getFilePath(arg1, &filePath))
		{
			TFE_System::logWrite(LOG_ERROR, "VUE", "key_appendVue: COULD NOT OPEN.");
			return frames;
		}
		TFE_Parser parser;
		if (!createParserFromFile(&filePath, &parser))
		{
			TFE_System::logWrite(LOG_ERROR, "VUE", "key_appendVue: COULD NOT OPEN.");
			return frames;
		}

		loadVueFile(frames, arg2, &parser);
		return frames;
	}

	void key_setViewFrames(VueLogic* vueLogic, Allocator* frames, s32 isCamera)
	{
		vueLogic->frames = frames;
		vueLogic->isCamera = isCamera;
		task_makeActive(vueLogic->task);
	}

	JBool vueLogicSetupFunc(Logic* logic, KEYWORD key)
	{
		VueLogic* vueLogic = (VueLogic*)logic;
		char* endPtr = nullptr;

		if (key == KW_VUE)
		{
			s32 isCamera = 0;
			if (s_objSeqArgCount < 3)
			{
				s_objSeqArg2[0] = '*';
			}
			else
			{
				_strupr(s_objSeqArg2);
				if (strstr(s_objSeqArg2, "CAMERA"))
				{
					isCamera = 1;
				}
			}
			Allocator* frames = key_loadVue(s_objSeqArg1, s_objSeqArg2, isCamera);
			key_setViewFrames(vueLogic, frames, isCamera);
			return JTRUE;
		}
		else if (key == KW_VUE_APPEND)
		{
			s32 isCamera = 0;
			if (s_objSeqArgCount < 3)
			{
				s_objSeqArg2[0] = '*';
			}
			else
			{
				_strupr(s_objSeqArg2);
				if (strstr(s_objSeqArg2, "CAMERA"))
				{
					isCamera = 1;
				}
			}
			key_appendVue(s_objSeqArg1, s_objSeqArg2, vueLogic->frames);
			return JTRUE;
		}
		else if (key == KW_FRAME_RATE)
		{
			vueLogic->frameDelay = time_frameRateToDelay(strtof(s_objSeqArg1, &endPtr));
			return JTRUE;
		}
		else if (key == KW_PAUSE)
		{
			vueLogic->flags |= VUE_PAUSED;
			return JTRUE;
		}
		return JFALSE;
	}

	// TODO(Core Game Loop Release): Stubbed out the minimum.
	void vueLogicTaskFunc(MessageType msg)
	{
		task_begin;

		task_yield(TASK_SLEEP);
		while (msg != MSG_FREE_TASK)
		{
			task_yield(TASK_NO_DELAY);
		}

		task_end;
	}

	void vueLogicCleanupFunc(Logic *logic)
	{
		// TODO(Core Game Loop Release)
	}

}  // TFE_DarkForces