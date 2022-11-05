#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Jedi/Math/core_math.h>
#include "robjData.h"

using namespace TFE_Jedi;

struct JediModel;
struct RSector;

namespace TFE_Jedi
{
	SecObject* allocateObject();
	void freeObject(SecObject* obj);

	// Spirits
	void spirit_setData(SecObject* obj);

	// 3D objects
	void obj3d_setData(SecObject* obj, JediModel* pod);
	void obj3d_computeTransform(SecObject* obj);

	// Sprites
	void sprite_setData(SecObject* obj, JediWax* data);

	// Frames
	void frame_setData(SecObject* obj, WaxFrame* data);

	extern JBool s_freeObjLock;
}
