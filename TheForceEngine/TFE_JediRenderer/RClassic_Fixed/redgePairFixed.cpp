#include "redgePairFixed.h"

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	void edgePair_setup(s32 length, s32 x0, fixed16_16 dyFloor_dx, fixed16_16 yFloor1, fixed16_16 yFloor, fixed16_16 dyCeil_dx, fixed16_16 yCeil, fixed16_16 yCeil1, EdgePair* edgePair)
	{
		const s32 yF0 = round16(yFloor);
		const s32 yF1 = round16(yFloor1);
		const s32 yC0 = round16(yCeil);
		const s32 yC1 = round16(yCeil1);

		edgePair->yCeil0.f16_16 = yCeil;
		edgePair->yCeil1.f16_16 = yCeil1;
		edgePair->dyCeil_dx.f16_16 = dyCeil_dx;
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

		edgePair->yFloor0.f16_16 = yFloor;
		edgePair->yFloor1.f16_16 = yFloor1;
		edgePair->dyFloor_dx.f16_16 = dyFloor_dx;
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
}  // REdgePair_Fixed

}  // TFE_JediRenderer