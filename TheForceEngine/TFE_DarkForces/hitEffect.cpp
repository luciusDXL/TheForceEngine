#include "hitEffect.h"
#include <TFE_Memory/allocator.h>

using namespace TFE_Memory;

namespace TFE_DarkForces
{
	struct HitEffect
	{
		HitEffectID type;
		RSector* sector;
		fixed16_16 x;
		fixed16_16 y;
		fixed16_16 z;
		SecObject* u14;
		s32 u18;
		s32 u1c;
	};

	static Allocator* s_hitEffects;

	void spawnHitEffect(HitEffectID hitEffectId, RSector* sector, fixed16_16 x, fixed16_16 y, fixed16_16 z, SecObject* u6c)
	{
		if (hitEffectId != HEFFECT_NONE)
		{
			HitEffect* effect = (HitEffect*)allocator_newItem(s_hitEffects);
			effect->type = hitEffectId;
			effect->sector = sector;
			effect->x = x;
			effect->y = y;
			effect->z = z;
			effect->u14 = u6c;
		}
	}
}  // TFE_DarkForces