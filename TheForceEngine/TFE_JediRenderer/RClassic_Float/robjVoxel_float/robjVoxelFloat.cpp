#include <TFE_System/profiler.h>
#include <TFE_Settings/settings.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Asset/voxelAsset.h>

#include "robjVoxelFloat.h"
#include "../../fixedPoint.h"
#include "../../rmath.h"
#include "../../rcommon.h"
#include "../../robject.h"
#include "../rlightingFloat.h"

#include "../fixedPoint20.h"

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	typedef void(*VoxelColumnFunc)();

	static s32 s_yPixelCount;
	static fixed44_20 s_vCoordStep;
	static fixed44_20 s_vCoordFixed;
	static const u8* s_columnLight;
	static VoxelColumnFunc s_columnFunc;
	static const u8* s_texImage;
	static u8* s_columnOut;

	static Vec2f s_columnCoordVS[4];
	static f32 s_cornersProjX[4];
	static f32 s_rcpZ[4];

	static s32 s_curFrame = 0;
	static s32 s_dbgIter = 0;
	static bool s_showDrawOrder = false;
	static s32 s_drawOrderStep = 0;

	static Vec2f s_boundsVS[4];
	static VoxelColumn* s_columns;
	static s32 s_columnCount;

	void drawTopCap(f32 y0, f32 y1, s32 i0, s32 width, s32 height, s32 depth, u8 color);
	void drawBotCap(f32 y0, f32 y1, s32 i0, s32 width, s32 height, s32 depth, u8 color);

	void debug_step(const std::vector<std::string>& args)
	{
		s_drawOrderStep = true;
		s32 count = 1;
		if (args.size() > 1)
		{
			char* endPtr = nullptr;
			count = max(1, strtol(args[1].c_str(), &endPtr, 10));
		}

		s_dbgIter += count;
	}

	void debug_continue(const std::vector<std::string>& args)
	{
		s_drawOrderStep = false;
	}

	void robjVoxel_init()
	{
		CVAR_BOOL(s_showDrawOrder, "rvoxel_showDrawOrder", CVFLAG_DO_NOT_SERIALIZE, "Show voxel draw order");
		CCMD("rvoxel_step", debug_step, 0, "rvoxel_step(count=1) - Step forward voxel drawing by 'step' count.");
		CCMD("rvoxel_continue", debug_continue, 0, "Continue iterating at full speed.");
	}

	void drawVoxelColumnLit()
	{
		const s32 end = s_yPixelCount - 1;
		const u8* tex = s_texImage;

		fixed44_20 vCoordFixed = s_vCoordFixed;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed);
			s_columnOut[offset] = s_columnLight[tex[v]];
		}
	}

	void drawVoxelColumnFullbright()
	{
		const s32 end = s_yPixelCount - 1;
		const u8* tex = s_texImage;

		fixed44_20 vCoordFixed = s_vCoordFixed;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width, vCoordFixed += s_vCoordStep)
		{
			const s32 v = floor20(vCoordFixed);
			s_columnOut[offset] = tex[v];
		}
	}

	void drawVoxelColumnColor(u8 color)
	{
		const s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			s_columnOut[offset] = color;
		}
	}

	void drawVoxelColumnDebug()
	{
		const s32 end = s_yPixelCount - 1;

		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			s_columnOut[offset] = 6;
		}
	}
		
	void renderSingleColumn(s32 x, f32 projY0, f32 projY1, f32 z, s32 width, s32 height, s32 depth)
	{
		if (z >= s_depth1d[x]) { return; }
		
		const s32 y0_pixel = roundFloat(projY0);
		const s32 y1_pixel = roundFloat(projY1);
		if (y0_pixel > s_windowMaxY || y1_pixel < s_windowMinY) { return; }

		const f32 projHeight = projY1 - projY0 + 1.0f;
		const f32 vCoordStep = f32(height) / projHeight;
		s_vCoordStep = floatToFixed20(vCoordStep);

		const s32 top = s_objWindowTop[x];
		const s32 bot = s_objWindowBot[x];
		const s32 y0clipped = y0_pixel < top ? top : y0_pixel;
		const s32 y1clipped = y1_pixel > bot ? bot : y1_pixel;

		s_yPixelCount = y1clipped - y0clipped + 1;
		if (s_yPixelCount > 0)
		{
			// TODO: The texture mapping is still slightly rough.
			const f32 vOffset = max(projY1 - f32(y1clipped), 0.0f);
			s_vCoordFixed = floatToFixed20(vOffset*vCoordStep);

			// Draw the column.
			s_columnOut = &s_display[y0clipped * s_width + x];
			s_columnFunc();
		}
	}

	void renderSingleColorColumn(s32 x, f32 projY0, f32 projY1, f32 z, s32 width, s32 height, s32 depth, u8 color)
	{
		if (z >= s_depth1d[x]) { return; }

		const s32 y0_pixel = roundFloat(projY0);
		const s32 y1_pixel = roundFloat(projY1);
		if (y0_pixel > s_windowMaxY || y1_pixel < s_windowMinY) { return; }

		const s32 top = s_objWindowTop[x];
		const s32 bot = s_objWindowBot[x];
		const s32 y0clipped = y0_pixel < top ? top : y0_pixel;
		const s32 y1clipped = y1_pixel > bot ? bot : y1_pixel;

		s_yPixelCount = y1clipped - y0clipped + 1;
		if (s_yPixelCount > 0)
		{
			// Draw the column.
			s_columnOut = &s_display[y0clipped * s_width + x];
			drawVoxelColumnColor(color);
		}
	}
	
	s32 getVoxelIndexFromSortOrder(s32 iter, s32 sortOrder, s32 width, s32 depth, s32* voxelX, s32* voxelZ)
	{
		// Determine the primary axis, and thus the way X and Z are decoded.
		*voxelX = (sortOrder & PRIM_X_AXIS) ? (iter % width) : (iter / depth);
		*voxelZ = (sortOrder & PRIM_Z_AXIS) ? (iter % depth) : (iter / width);
		// Determine the sort direction for each axis, flipping the results if needed.
		if (sortOrder & NEG_X_AXIS) { *voxelX = width - (*voxelX) - 1; }
		if (sortOrder & NEG_Z_AXIS) { *voxelZ = depth - (*voxelZ) - 1; }
		// Final column index.
		return (*voxelZ) * width + (*voxelX);
	}

	// Draw the voxel volume, assuming yaw only transformation and "classic" renderer.
	void robjVoxel_drawClassic(SecObject* obj, VoxelModel* model)
	{
		// Dimensions of the voxel object (in voxels).
		const s32 depth = model->depth;
		const s32 width = model->width;
		const s32 height = model->height;
				
		// Dimensions of the voxel object (in world units).
		const f32 widthWS = model->worldWidth;
		const f32 depthWS = model->worldDepth;
		const f32 heightWS = model->worldHeight;
				
		// 2D bounds of the voxel object in local space (relative to the camera).
		const Vec2f boundsRel[4] =
		{
			{-widthWS*0.5f + obj->posWS.x.f32 - s_cameraPosX,  depthWS*0.5f + obj->posWS.z.f32 - s_cameraPosZ},
			{ widthWS*0.5f + obj->posWS.x.f32 - s_cameraPosX,  depthWS*0.5f + obj->posWS.z.f32 - s_cameraPosZ},
			{ widthWS*0.5f + obj->posWS.x.f32 - s_cameraPosX, -depthWS*0.5f + obj->posWS.z.f32 - s_cameraPosZ},
			{-widthWS*0.5f + obj->posWS.x.f32 - s_cameraPosX, -depthWS*0.5f + obj->posWS.z.f32 - s_cameraPosZ}
		};

		// 2D bounds of the voxel object in viewspace.
		// To add arbitrary yaw, concatenate the object and view matrices.
		for (s32 i = 0; i < 4; i++)
		{
			s_boundsVS[i].x = (boundsRel[i].x*s_cameraMtx[0]) + (boundsRel[i].z*s_cameraMtx[6]);
			s_boundsVS[i].z = (boundsRel[i].x*s_cameraMtx[2]) + (boundsRel[i].z*s_cameraMtx[8]);
		}

		// Column Data.
		s_columns = model->columns;
		s_columnCount = width * depth;

		// Compute the grid back to front draw order from the view direction.
		// This forms an S x T coordinate space that maps from viewspace back to local space.
		const Vec2f dir = { obj->posWS.x.f32 - s_cameraPosX, obj->posWS.z.f32 - s_cameraPosZ };
		u32 sortOrder = 0;
		if (fabsf(dir.x) < fabsf(dir.z))
		{
			// 'S' axis
			if (dir.x < 0.0f) { sortOrder |= POS_X; }
			else { sortOrder |= NEG_X; }
			// 'T' axis
			if (dir.z > 0.0f) { sortOrder |= (POS_Z << 4); }
			else { sortOrder |= (NEG_Z << 4); }
		}
		else
		{
			// 'S' axis
			if (dir.z < 0.0f) { sortOrder |= NEG_Z; }
			else { sortOrder |= POS_Z; }
			// 'T' axis
			if (dir.x < 0.0f) { sortOrder |= (POS_X << 4); }
			else { sortOrder |= (NEG_X << 4); }
		}

		// Voxel axis in viewspace, used to step along the grid and compute vertices from the grid position.
		const f32 voxelSize = 0.1f;	// This should vary with world space scale.
		const Vec2f xStep =
		{
			voxelSize * s_cameraMtx[0],
			voxelSize * s_cameraMtx[2]
		};
		const Vec2f zStep =
		{
			voxelSize * s_cameraMtx[6],
			voxelSize * s_cameraMtx[8]
		};
		
		const f32 y0Base = obj->posVS.y.f32;
		// Debug feature, normally count = s_columnCount.
		const s32 count = s_showDrawOrder ? ((s_dbgIter) % (s_columnCount+1)) : s_columnCount;

		// half voxel Z offset, for computing voxel centroid Z values (for lighting).
		const f32 halfZStep = (xStep.z + zStep.z)*0.5f;
		// Local grid coordinates for computing face normals.
		const Vec2f local[4] =
		{
			{ 0.0f, 0.0f },
			{ 1.0f, 0.0f },
			{ 1.0f, 1.0f },
			{ 0.0f, 1.0f }
		};

		// Loop through the voxel columns.
		for (s32 c = 0; c < count; c++)
		{
			// From the current index 'c', and given the sort order computed before, get the current grid position and column index.
			s32 voxelX, voxelZ;
			const s32 voxelIndex = getVoxelIndexFromSortOrder(c, sortOrder, width, depth, &voxelX, &voxelZ);

			// Get the column data - skipping the column if it is empty.
			const u32 dataSize = s_columns[voxelIndex].dataSize & 0xffff;
			const u8* data     = s_columns[voxelIndex].data;
			const u8* dataEnd  = s_columns[voxelIndex].data + dataSize;
			const u8* subColumnMask = dataEnd;
			if (!dataSize) { continue; }

			// Calculate the column coordinates in viewspace.
			// Calculate the column corner from the position on the grid (voxelX, voxelZ).
			s_columnCoordVS[0] = { s_boundsVS[0].x + xStep.x*f32(voxelX) - zStep.x*f32(voxelZ), s_boundsVS[0].z + xStep.z*f32(voxelX) - zStep.z*f32(voxelZ)};
			// Then compute the 3 additional corners of the rectangular solid based on the coordinate system computed earlier.
			s_columnCoordVS[1] = { s_columnCoordVS[0].x + xStep.x,           s_columnCoordVS[0].z + xStep.z };
			s_columnCoordVS[2] = { s_columnCoordVS[0].x + xStep.x + zStep.x, s_columnCoordVS[0].z + xStep.z + zStep.z };
			s_columnCoordVS[3] = { s_columnCoordVS[0].x + zStep.x,           s_columnCoordVS[0].z + zStep.z };
						
			// Calculate the X projection for each corner and determine min projected X and viewspace Z.
			f32 xMin = FLT_MAX, xMax = -FLT_MAX, zMin = FLT_MAX;
			s32 leftIndex = -1;
			for (s32 i = 0; i < 4; i++)
			{
				// Cull the voxel column if any vertices are behind the near plane.
				if (s_columnCoordVS[i].z < 1.0f) { zMin = s_columnCoordVS[i].z; break; }

				zMin = min(zMin, s_columnCoordVS[i].z);
				s_rcpZ[i] = 1.0f / s_columnCoordVS[i].z;
				s_cornersProjX[i] = s_columnCoordVS[i].x*s_focalLength*s_rcpZ[i] + s_halfWidth;

				if (s_cornersProjX[i] < xMin)
				{
					xMin = s_cornersProjX[i];
					leftIndex = i;
				}
				xMax = max(xMax, s_cornersProjX[i]);
			}
			// Cull columns that are outside of the current view window or behind the near plane.
			if (zMin < 1.0f || xMax < s_windowMinX || xMin > s_windowMaxX) { continue; }

			// Determine the vertex order based on the direction we are viewing the column.
			// Basically assuming we start at the left most point in screenspace, we can walk along the top edges (left, left+3, left+2) and bottom edges (left, left+1, left+2)
			const s32 i0 = leftIndex, i1 = (leftIndex + 1) & 3, i2 = (leftIndex + 2) & 3, i3 = (leftIndex + 3) & 3;

			// Calculate the face normals and determine if the major face is first or second.
			const Vec2f faceNrml[]=
			{
				{ -local[i1].z + local[i0].z, -local[i1].x + local[i0].x },
				{ -local[i2].z + local[i1].z, -local[i2].x + local[i1].x }
			};
			s32 axis;
			if (!(sortOrder&PRIM_X_AXIS))
			{
				axis = fabsf(faceNrml[0].x) > fabsf(faceNrml[1].x) ? 0 : 1;
			}
			else
			{
				axis = fabsf(faceNrml[0].z) > fabsf(faceNrml[1].z) ? 0 : 1;
			}
			const Vec2f* minorNrml = &faceNrml[!axis];

			// determine which front face extends along the major axis.
			const s32 majorFace = axis;
			const s32 minorFace = !axis;
			const s32 majorFaceMask = (sortOrder >> 4) & 15;	// This is always the minor axis - 'T' in the S x T coordinate space.

			// The 'S' coordinate in our S x T space can flip based on where the voxel is in relation to the camera.
			// This is due to perspective projection.
			s32 minorFaceMask = sortOrder & 15;
			     if (minorNrml->x < -0.5f) { minorFaceMask = POS_X; }
			else if (minorNrml->x >  0.5f) { minorFaceMask = NEG_X; }
			else if (minorNrml->z < -0.5f) { minorFaceMask = POS_Z; }
			else if (minorNrml->z >  0.5f) { minorFaceMask = NEG_Z; }

			// Single light level per voxel (based on centroid Z).
			s_columnLight = computeLighting(halfZStep + s_columnCoordVS[0].z, 0);
			s_columnFunc = s_columnLight ? drawVoxelColumnLit : drawVoxelColumnFullbright;
						
			// Render sub-columns, using RLE compression to skip empty space.
			u32 subColumnIndex = 0;
			for (s32 yCur = 0; data != dataEnd;)
			{
				const u8 count = *data;
				data++;

				// Is this an empty Sub-column?
				if (count & 0x80)
				{
					// Skip past the empty voxels.
					yCur += (count & 0x7f);
					continue;
				}

				// Otherwise render the Sub-column.
				const f32 y1 = y0Base - f32(yCur) * 0.1f;
				const f32 y0 = y1 - f32(count) * 0.1f;
				// Determine which cap, if any, is required for the sub-column.
				const bool hasTopCap = y0 > 0.0f;
				const bool hasBotCap = y1 < 0.0f;

				// Color data from RLE stream.
				const u8* colorData = data;
				data += count;
				// Adjacency mask data from RLE stream.
				const u8* adjacencyMask = data;
				data += ((count + 1) >> 1);	// Adjacency data is packed 2 voxels per byte, so we have to round up the next whole byte.

				// Get the sub-column "cap" mask - this determines which caps might be visible.
				const u8 capMask = (subColumnMask[subColumnIndex>>2] >> ((subColumnIndex&3)*2)) & 3;

				// Render the top cap, if required.
				if (hasTopCap && (capMask & POS_Y))
				{
					// Read color from the top of the column.
					const u8 color = s_columnLight ? s_columnLight[colorData[count - 1]] : colorData[count - 1];
					// Render solid color cap (diamond).
					drawTopCap(y0, y1, i0, width, height, depth, color);
				}
				// Render the sub-column front faces.
				for (s32 i = 0; i < 2; i++)
				{
					// Front face indices.
					const s32 frontI0 = (i0 + i) & 3;
					const s32 frontI1 = (i0 + i + 1) & 3;

					// Z and 1/Z values for this face.
					const f32 z0 = s_columnCoordVS[frontI0].z;
					const f32 z1 = s_columnCoordVS[frontI1].z;
					const f32 rcpZ0 = s_rcpZ[frontI0];
					const f32 rcpZ1 = s_rcpZ[frontI1];
					// Projected X coordinates for this face.
					const f32 projX0 = s_cornersProjX[frontI0];
					const f32 projX1 = s_cornersProjX[frontI1];

					const s32 x0_pixel = max(roundFloat(projX0), s_windowMinX);
					const s32 x1_pixel = min(roundFloat(projX1), s_windowMaxX);
					const s32 length = x1_pixel - x0_pixel + 1;
					// Skip if the projected width is too small or if the face is backfacing.
					if (length < 1) { continue; }

					// Calculate the x range scale, the xOffset for (offset between rounded pixel position and true position).
					const f32 xDeltaScale = 1.0f / (projX1 - projX0 + 1.0f);
					const f32 xOffset = f32(x0_pixel) - projX0;
					const f32 dZdX = (z1 - z0) * xDeltaScale;

					// Now split again into sub-columns again based on the adjacency mask data.
					// This will make sure that faces shared between voxels are not rendered.
					const s32 faceMask = (i == minorFace) ? minorFaceMask : majorFaceMask;
					const s32 faceMaskUpper = faceMask << 4;
					const s32 faceMaskPair = faceMask | faceMaskUpper;
					s32 vCur = 0;
					while (vCur < count)
					{
						const u8* mask = &adjacencyMask[vCur >> 1];
						s32 vStart = count;
						s32 loopStart = vCur;

						// Skip empty space, look for the first non-empty voxel face.
						if (loopStart & 1)
						{
							if ((*mask) & faceMaskUpper) { vStart = vCur; }
							mask++;
							loopStart++;
						}
						for (s32 v = loopStart; v < count && vStart == count; v += 2, mask++)
						{
							const u8 m = *mask;
							if (m & faceMaskPair)
							{
								vStart = v + (!(m & faceMask));	// add 1 if 'v' is empty.
								break;
							}
						}
						// We are at the end, no more voxels to render.
						if (vStart >= count) { break; }

						s32 vEnd = count;
						loopStart = vStart + 1;
						mask = &adjacencyMask[loopStart >> 1];
						if (vStart & 1)
						{
							if (!((*mask)&faceMaskUpper)) { vEnd = loopStart; }
							loopStart++;
							mask++;
						}
						for (s32 v = loopStart; v < count && vEnd == count; v += 2, mask++)
						{
							const u8 m = *mask;
							if (!(m&faceMask) || !(m&faceMaskUpper))
							{
								vEnd = v + (!!(m&faceMask));	// add 1 if 'v' is solid.
								break;
							}
						}
						vEnd = min(vEnd, count);
						const s32 texelHeight = vEnd - vStart;
						s_texImage = colorData + vStart;
						vCur += texelHeight;

						// render sub-column between [vStart, vEnd).
						const f32 y1Sub = y0Base - f32(yCur + vStart) * 0.1f;
						const f32 y0Sub = y1Sub  - f32(texelHeight) * 0.1f;

						// Projected Y0 (top of the column) for both vertices of the edge.
						const f32 projY00 = y0Sub * s_focalLenAspect*rcpZ0 + s_halfHeight;
						const f32 projY01 = y1Sub * s_focalLenAspect*rcpZ0 + s_halfHeight;
						// Project Y1 (bottom of the column) for both vertices of the edge.
						const f32 projY10 = y0Sub * s_focalLenAspect*rcpZ1 + s_halfHeight;
						const f32 projY11 = y1Sub * s_focalLenAspect*rcpZ1 + s_halfHeight;
						// dYdX for the Y0 and Y1 values along the edge.
						const f32 dY0dX = (projY10 - projY00) * xDeltaScale;
						const f32 dY1dX = (projY11 - projY01) * xDeltaScale;
						// initial z and Y coordinates.
						f32 z = z0 + dZdX * xOffset;
						f32 projY0 = projY00 + dY0dX * xOffset;
						f32 projY1 = projY01 + dY1dX * xOffset;

						// Render a textured line from the top of the voxel sub-column to the bottom for each
						// screenspace X coordinate.
						for (s32 x = x0_pixel; x <= x1_pixel; x++, z += dZdX, projY0 += dY0dX, projY1 += dY1dX)
						{
							renderSingleColumn(x, projY0, projY1, z, width, texelHeight, depth);
						}
					};
				}
				// Render the bottom cap if required.
				if (hasBotCap && (capMask & NEG_Y))
				{
					// Read color
					const u8 color = s_columnLight ? s_columnLight[colorData[0]] : colorData[0];
					drawBotCap(y0, y1, i0, width, height, depth, color);
				}
				// Continue along the column.
				yCur += count;
				subColumnIndex++;
			}
		}
	}
		
	void robjVoxel_draw(SecObject* obj, VoxelModel* model)
	{
		// Debug
		if (s_frame != s_curFrame)
		{
			s_curFrame = s_frame;
			if (!s_drawOrderStep) { s_dbgIter++; }
		}

		// Main draw - for now only the "yaw only" version.
		robjVoxel_drawClassic(obj, model);
	}

	/////////////////////////////////////////////////////////////////////
	// Cap drawing code can use massive improvement and cleanup.
	// But it is a "just get it working" first pass.
	/////////////////////////////////////////////////////////////////////

	// Compute a single color for the cap.
	void drawTopCap(f32 y0, f32 y1, s32 i0, s32 width, s32 height, s32 depth, u8 color)
	{
		// We have 4 points and 4 edges.
		// Project all 4 points (x) and compute the lengths.
		f32 y0Proj[4];
		f32 y1Proj[4];
		for (s32 i = 0; i < 4; i++)
		{
			y0Proj[i] = y0 * s_focalLenAspect*s_rcpZ[i] + s_halfHeight;
			y1Proj[i] = y1 * s_focalLenAspect*s_rcpZ[i] + s_halfHeight;
		}

		const s32 backI[] =
		{
			(4 + i0) & 3,
			(4 + i0 - 1) & 3,
			(4 + i0 - 2) & 3
		};
		const s32 frontI[] =
		{
			(i0) & 3,
			(i0 + 1) & 3,
			(i0 + 2) & 3,
		};

		s32 xb[3] =
		{
			floorFloat(s_cornersProjX[backI[0]]),
			floorFloat(s_cornersProjX[backI[1]]),
			floorFloat(s_cornersProjX[backI[2]])
		};
		s32 xf[3] =
		{
			xb[0],
			floorFloat(s_cornersProjX[frontI[1]]),
			xb[2]
		};

		s32 lengthBack[] =
		{
			min(xb[1], s_windowMaxX) - max(xb[0], s_windowMinX) + 1,
			min(xb[2], s_windowMaxX) - max(xb[1], s_windowMinX) + 1
		};
		s32 lengthFront[] =
		{
			min(xf[1], s_windowMaxX) - max(xf[0], s_windowMinX) + 1,
			min(xf[2], s_windowMaxX) - max(xf[1], s_windowMinX) + 1
		};
		s32 curBackEdge = lengthBack[0] > 0 ? 0 : 1;
		s32 curFrontEdge = lengthFront[0] > 0 ? 0 : 1;
		if ((lengthBack[0] < 1 && lengthBack[1] < 1) || (lengthFront[0] < 1 && lengthFront[1] < 1)) { return; }

		f32 xScaleBack = 1.0f / (s_cornersProjX[backI[1 + curBackEdge]] - s_cornersProjX[backI[0 + curBackEdge]] + 1.0f);
		f32 xScaleFront = 1.0f / (s_cornersProjX[frontI[1 + curFrontEdge]] - s_cornersProjX[frontI[0 + curFrontEdge]] + 1.0f);
		f32 dZbdX = (s_columnCoordVS[backI[1 + curBackEdge]].z - s_columnCoordVS[backI[0 + curBackEdge]].z) * xScaleBack;
		f32 dZfdX = (s_columnCoordVS[frontI[1 + curFrontEdge]].z - s_columnCoordVS[frontI[0 + curFrontEdge]].z) * xScaleFront;
		f32 dY0bdX = (y0Proj[backI[1 + curBackEdge]] - y0Proj[backI[0 + curBackEdge]]) * xScaleBack;
		f32 dY0fdX = (y0Proj[frontI[1 + curFrontEdge]] - y0Proj[frontI[0 + curFrontEdge]]) * xScaleFront;

		f32 xOffset = f32(xb[0]) - s_cornersProjX[backI[curBackEdge]];
		f32 zb = s_columnCoordVS[backI[0 + curBackEdge]].z + dZbdX * xOffset;
		f32 zf = s_columnCoordVS[frontI[0 + curFrontEdge]].z + dZfdX * xOffset;

		f32 projY0b = y0Proj[backI[0 + curBackEdge]] + dY0bdX * xOffset;
		f32 projY0f = y0Proj[frontI[0 + curFrontEdge]] + dY0fdX * xOffset;

		s32 lB = lengthBack[curBackEdge];
		s32 lF = lengthFront[curFrontEdge];

		s32 x = xb[0];
		while (curBackEdge < 2 && curFrontEdge < 2 && x <= xb[2] + 1)
		{
			if (x >= s_windowMinX && x <= s_windowMaxX)
			{
				renderSingleColorColumn(x, projY0b, projY0f, zb, width, height, depth, color);
			}

			if (lB)
			{
				projY0b += dY0bdX;
				zb += dZbdX;
				lB--;
			}
			else
			{
				curBackEdge++;
				if (curBackEdge > 1) { break; }

				xScaleBack = 1.0f / (s_cornersProjX[backI[1 + curBackEdge]] - s_cornersProjX[backI[0 + curBackEdge]] + 1.0f);
				dZbdX = (s_columnCoordVS[backI[1 + curBackEdge]].z - s_columnCoordVS[backI[0 + curBackEdge]].z) * xScaleBack;
				dY0bdX = (y0Proj[backI[1 + curBackEdge]] - y0Proj[backI[0 + curBackEdge]]) * xScaleBack;
				xOffset = f32(x + 1) - s_cornersProjX[backI[curBackEdge]];
				zb = s_columnCoordVS[backI[0 + curBackEdge]].z + dZbdX * xOffset;
				projY0b = y0Proj[backI[0 + curBackEdge]] + dY0bdX * xOffset;
				lB = lengthBack[curBackEdge];
			}

			if (lF)
			{
				projY0f += dY0fdX;
				lF--;
			}
			else
			{
				curFrontEdge++;
				if (curFrontEdge > 1) { break; }

				xScaleFront = 1.0f / (s_cornersProjX[frontI[1 + curFrontEdge]] - s_cornersProjX[frontI[0 + curFrontEdge]] + 1.0f);
				dY0fdX = (y0Proj[frontI[1 + curFrontEdge]] - y0Proj[frontI[0 + curFrontEdge]]) * xScaleFront;
				xOffset = f32(x + 1) - s_cornersProjX[frontI[curFrontEdge]];
				projY0f = y0Proj[frontI[0 + curFrontEdge]] + dY0fdX * xOffset;
				lF = lengthFront[curFrontEdge];
			}

			x++;
		}
	}

	// Compute a single color for the cap.
	void drawBotCap(f32 y0, f32 y1, s32 i0, s32 width, s32 height, s32 depth, u8 color)
	{
		// We have 4 points and 4 edges.
		// Project all 4 points (x) and compute the lengths.
		f32 y0Proj[4];
		f32 y1Proj[4];
		for (s32 i = 0; i < 4; i++)
		{
			y0Proj[i] = y0 * s_focalLenAspect*s_rcpZ[i] + s_halfHeight;
			y1Proj[i] = y1 * s_focalLenAspect*s_rcpZ[i] + s_halfHeight;
		}

		const s32 backI[] =
		{
			(4 + i0) & 3,
			(4 + i0 - 1) & 3,
			(4 + i0 - 2) & 3
		};
		const s32 frontI[] =
		{
			(i0) & 3,
			(i0 + 1) & 3,
			(i0 + 2) & 3,
		};

		s32 xb[3] =
		{
			floorFloat(s_cornersProjX[backI[0]]),
			floorFloat(s_cornersProjX[backI[1]]),
			floorFloat(s_cornersProjX[backI[2]])
		};
		s32 xf[3] =
		{
			xb[0],
			floorFloat(s_cornersProjX[frontI[1]]),
			xb[2]
		};

		s32 lengthBack[] =
		{
			min(xb[1], s_windowMaxX) - max(xb[0], s_windowMinX) + 1,
			min(xb[2], s_windowMaxX) - max(xb[1], s_windowMinX) + 1
		};
		s32 lengthFront[] =
		{
			min(xf[1], s_windowMaxX) - max(xf[0], s_windowMinX) + 1,
			min(xf[2], s_windowMaxX) - max(xf[1], s_windowMinX) + 1
		};
		s32 curBackEdge = lengthBack[0] > 0 ? 0 : 1;
		s32 curFrontEdge = lengthFront[0] > 0 ? 0 : 1;
		if ((lengthBack[0] < 1 && lengthBack[1] < 1) || (lengthFront[0] < 1 && lengthFront[1] < 1)) { return; }

		f32 xScaleBack = 1.0f / (s_cornersProjX[backI[1 + curBackEdge]] - s_cornersProjX[backI[0 + curBackEdge]] + 1.0f);
		f32 xScaleFront = 1.0f / (s_cornersProjX[frontI[1 + curFrontEdge]] - s_cornersProjX[frontI[0 + curFrontEdge]] + 1.0f);
		f32 dZbdX = (s_columnCoordVS[backI[1 + curBackEdge]].z - s_columnCoordVS[backI[0 + curBackEdge]].z) * xScaleBack;
		f32 dZfdX = (s_columnCoordVS[frontI[1 + curFrontEdge]].z - s_columnCoordVS[frontI[0 + curFrontEdge]].z) * xScaleFront;
		f32 dY1bdX = (y1Proj[backI[1 + curBackEdge]] - y1Proj[backI[0 + curBackEdge]]) * xScaleBack;
		f32 dY1fdX = (y1Proj[frontI[1 + curFrontEdge]] - y1Proj[frontI[0 + curFrontEdge]]) * xScaleFront;

		f32 xOffset = f32(xb[0]) - s_cornersProjX[backI[curBackEdge]];
		f32 zb = s_columnCoordVS[backI[0 + curBackEdge]].z + dZbdX * xOffset;
		f32 zf = s_columnCoordVS[frontI[0 + curFrontEdge]].z + dZfdX * xOffset;

		f32 projY1b = y1Proj[backI[0 + curBackEdge]] + dY1bdX * xOffset;
		f32 projY1f = y1Proj[frontI[0 + curFrontEdge]] + dY1fdX * xOffset;

		s32 lB = lengthBack[curBackEdge];
		s32 lF = lengthFront[curFrontEdge];

		s32 x = xb[0];
		while (curBackEdge < 2 && curFrontEdge < 2 && x <= xb[2] + 1)
		{
			if (x >= s_windowMinX && x <= s_windowMaxX)
			{
				renderSingleColorColumn(x, projY1f, projY1b, zb, width, height, depth, color);
			}
			if (lB)
			{
				projY1b += dY1bdX;
				zb += dZbdX;
				lB--;
			}
			else
			{
				curBackEdge++;
				if (curBackEdge > 1) { break; }

				xScaleBack = 1.0f / (s_cornersProjX[backI[1 + curBackEdge]] - s_cornersProjX[backI[0 + curBackEdge]] + 1.0f);
				dZbdX = (s_columnCoordVS[backI[1 + curBackEdge]].z - s_columnCoordVS[backI[0 + curBackEdge]].z) * xScaleBack;
				dY1bdX = (y1Proj[backI[1 + curBackEdge]] - y1Proj[backI[0 + curBackEdge]]) * xScaleBack;
				xOffset = f32(x + 1) - s_cornersProjX[backI[curBackEdge]];
				zb = s_columnCoordVS[backI[0 + curBackEdge]].z + dZbdX * xOffset;
				projY1b = y1Proj[backI[0 + curBackEdge]] + dY1bdX * xOffset;
				lB = lengthBack[curBackEdge];
			}

			if (lF)
			{
				projY1f += dY1fdX;
				lF--;
			}
			else
			{
				curFrontEdge++;
				if (curFrontEdge > 1) { break; }

				xScaleFront = 1.0f / (s_cornersProjX[frontI[1 + curFrontEdge]] - s_cornersProjX[frontI[0 + curFrontEdge]] + 1.0f);
				dY1fdX = (y1Proj[frontI[1 + curFrontEdge]] - y1Proj[frontI[0 + curFrontEdge]]) * xScaleFront;
				xOffset = f32(x + 1) - s_cornersProjX[frontI[curFrontEdge]];
				projY1f = y1Proj[frontI[0 + curFrontEdge]] + dY1fdX * xOffset;
				lF = lengthFront[curFrontEdge];
			}

			x++;
		}
	}

}}  // TFE_JediRenderer