#pragma once
#include <TFE_System/types.h>

enum AudioUpsampleFilter
{
	AUF_NONE = 0,
	AUF_LINEAR,
	AUF_COUNT,
	AUF_DEFAULT = AUF_LINEAR
};

namespace TFE_Audio
{
	void upsample4x_point(f32* output, const f32* input, s32 inputSampleCount);
	void upsample4x_linear(f32* output, const f32* input, s32 inputSampleCount);
}
