#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Reticle Support
// Support that can be used by all games.
//////////////////////////////////////////////////////////////////////
#include <TFE_Settings/gameSourceData.h>
#include <TFE_Memory/memoryRegion.h>

bool reticle_init();
void reticle_destroy();

void reticle_enable(bool enable);
void reticle_setShape(u32 index);
void reticle_setColor(const f32* color);
void reticle_setScale(f32 scale);

bool reticle_enabled();
u32  reticle_getShape();
u32  reticle_getShapeCount();
void reticle_getColor(f32* color);
f32  reticle_getScale();
