#include "redgePairFloat.h"

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	void edgePair_setup(s32 length, s32 x0, f32 dyFloor_dx, f32 yFloor1, f32 yFloor, f32 dyCeil_dx, f32 yCeil, f32 yCeil1, EdgePair* edgePair)
	{
		const s32 yF0 = roundFloat(yFloor);
		const s32 yF1 = roundFloat(yFloor1);
		const s32 yC0 = roundFloat(yCeil);
		const s32 yC1 = roundFloat(yCeil1);

		edgePair->yCeil0.f32 = yCeil;
		edgePair->yCeil1.f32 = yCeil1;
		edgePair->dyCeil_dx.f32 = dyCeil_dx;
		if (yC0 < yC1)
		{
			edgePair->yPixel_C0 = yC0;
			edgePair->yPixel_C1 = yC1;
		}
		else
		{
			edgePair->yPixel_C0 = yC1;
			edgePair->yPixel_C1 = yC0;
		}

		edgePair->yFloor0.f32 = yFloor;
		edgePair->yFloor1.f32 = yFloor1;
		edgePair->dyFloor_dx.f32 = dyFloor_dx;
		if (yF0 > yF1)
		{
			edgePair->yPixel_F0 = yF0;
			edgePair->yPixel_F1 = yF1;
		}
		else
		{
			edgePair->yPixel_F0 = yF1;
			edgePair->yPixel_F1 = yF0;
		}

		edgePair->lengthInPixels = length;
		edgePair->x0 = x0;
		edgePair->x1 = x0 + length - 1;
	}
}  // RClassic_Float

}  // TFE_JediRenderer