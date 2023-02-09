#include <cstring>
#include "audioFilters.h"

namespace TFE_Audio
{
	void upsample4x_point(f32* output, const f32* input, s32 inputSampleCount)
	{
		for (s32 i = 0; i < inputSampleCount; i += 2, output += 8, input += 2)
		{
			const f32 inLeft  = input[0];
			const f32 inRight = input[1];

			output[0] = inLeft;
			output[1] = inRight;

			output[2] = inLeft;
			output[3] = inRight;

			output[4] = inLeft;
			output[5] = inRight;

			output[6] = inLeft;
			output[7] = inRight;
		}
	}

	void upsample4x_linear(f32* output, const f32* input, s32 inputSampleCount)
	{
		// Read the current input and next input, then interpolate between them.
		// Note it is safe to read the next input because the callback *oversamples* by 2 samples (really 1 stereo sample).
		// Simple linear interpolation: sample0 + u*(sample1 - sample0),
		// where u = subsampleIndex / 4.0 (note if we upsample by something other than 4x in the future, this will need to be changed).
		for (s32 i = 0; i < inputSampleCount; i += 2, input += 2, output += 8)
		{
			const f32 inLeft0    = input[0];
			const f32 inRight0   = input[1];
			const f32 deltaLeft  = input[2] - inLeft0;
			const f32 deltaRight = input[3] - inRight0;

			output[0] = inLeft0;
			output[1] = inRight0;

			output[2] = inLeft0  + deltaLeft  * 0.25f;
			output[3] = inRight0 + deltaRight * 0.25f;

			output[4] = inLeft0  + deltaLeft  * 0.5f;
			output[5] = inRight0 + deltaRight * 0.5f;

			output[6] = inLeft0  + deltaLeft  * 0.75f;
			output[7] = inRight0 + deltaRight * 0.75f;
		}
	}
}
