#include "editGuidelines.h"
#include "editGeometry.h"
#include "hotkeys.h"
#include "error.h"
#include "infoPanel.h"
#include "levelEditor.h"
#include "levelEditorData.h"
#include "levelEditorHistory.h"
#include "sharedState.h"
#include "selection.h"
#include "userPreferences.h"
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/LevelEditor/Rendering/grid.h>
#include <TFE_Input/input.h>

#include <climits>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace LevelEditor
{
	/////////////////////////////////////////////////////
	// Internal Variables
	/////////////////////////////////////////////////////

	/////////////////////////////////////////////////////
	// Shared Variables
	/////////////////////////////////////////////////////
	GuidelinesEdit s_editGuidelines;

	/////////////////////////////////////////////////////
	// Forward Declarations.
	/////////////////////////////////////////////////////
	void createGuidlinesFromRect();
	void createGuidelinesFromShape();
	void selectGuideline();
	void findHoveredGuideline();

	void editGuidelines_init()
	{
		s_editGuidelines = {};

		s_editGuidelines.guidelines.bounds = { 0 };
		s_editGuidelines.guidelines.edge.clear();
		s_editGuidelines.guidelines.vtx.clear();
	}

	void removeLastEdge()
	{
		// Remove the last place edge.
		if (!s_editGuidelines.guidelines.edge.empty())
		{
			// Remove idx[1] and idx[2]
			GuidelineEdge& edge = s_editGuidelines.guidelines.edge.back();
			if (edge.idx[2] >= 0)
			{
				s_editGuidelines.guidelines.vtx.erase(s_editGuidelines.guidelines.vtx.begin() + edge.idx[2]);
			}
			s_editGuidelines.guidelines.vtx.erase(s_editGuidelines.guidelines.vtx.begin() + edge.idx[1]);
			s_editGuidelines.guidelines.edge.pop_back();
		}
		else
		{
			s_editGuidelines.drawStarted = false;
			s_editGuidelines.shapeComplete = false;
		}
	}

	void handleGuidelinesEdit(RayHitInfo* hitInfo)
	{
		if (s_editorConfig.interfaceFlags & PIF_HIDE_GUIDELINES)
		{
			return;
		}

		// Snap the cursor to the grid.
		Vec2f onGrid = { s_cursor3d.x, s_cursor3d.z };

		// TODO: What kind of snapping is supported?
		if (s_view == EDIT_VIEW_2D)
		{
			snapToGrid(&onGrid);
		}
		else
		{
			// snap to the closer: 3d cursor position or grid intersection.
			Vec3f posOnGrid;
			if (gridCursorIntersection(&posOnGrid) && s_editGuidelines.drawStarted)
			{
				s_cursor3d = posOnGrid;
				onGrid = { s_cursor3d.x, s_cursor3d.z };
			}
			snapToGrid(&onGrid);
		}
		s_cursor3d.x = onGrid.x;
		s_cursor3d.z = onGrid.z;

		s_hoveredGuideline = -1;
		if (TFE_Input::keyModDown(KeyModifier::KEYMOD_CTRL))
		{
			findHoveredGuideline();
		}
		if (isShortcutPressed(SHORTCUT_DELETE))
		{
			if (s_curGuideline >= 0)
			{
				guideline_delete(s_curGuideline);
			}
			else if (s_hoveredGuideline >= 0)
			{
				guideline_delete(s_hoveredGuideline);
			}
		}

		// Two ways to draw: rectangle (shift + left click and drag, escape to cancel), shape (left click to start, backspace to go back one point, escape to cancel)
		if (s_editGuidelines.drawStarted)
		{
			s_editGuidelines.drawCurPos = onGrid;
			if (s_editGuidelines.drawMode == DMODE_RECT)
			{
				s_editGuidelines.guidelines.vtx[1] = onGrid;
				if (!TFE_Input::mouseDown(MouseButton::MBUTTON_LEFT))
				{
					createGuidlinesFromRect();
				}
			}
			else if (s_editGuidelines.drawMode == DMODE_SHAPE)
			{
				if (s_singleClick)
				{
					if (s_drawActions & DRAW_ACTION_CURVE)
					{
						// New mode: place control point.
						s_editGuidelines.drawMode = DMODE_CURVE_CONTROL;
						// Compute the starting control point position.
						s_editGuidelines.controlPoint.x = (onGrid.x + s_editGuidelines.guidelines.vtx.back().x) * 0.5f;
						s_editGuidelines.controlPoint.z = (onGrid.z + s_editGuidelines.guidelines.vtx.back().z) * 0.5f;
					}

					if (TFE_Polygon::vtxEqual(&onGrid, &s_editGuidelines.guidelines.vtx[0]))
					{
						if (s_editGuidelines.guidelines.edge.empty())
						{
							s_editGuidelines.drawStarted = false;
							s_editGuidelines.shapeComplete = false;
						}
						else if (s_editGuidelines.drawMode == DMODE_SHAPE)
						{
							s32 index0 = s_editGuidelines.guidelines.edge.back().idx[1];
							s_editGuidelines.guidelines.edge.push_back({ index0, 0, -1 });
							createGuidelinesFromShape();
						}
						else if (s_editGuidelines.drawMode == DMODE_CURVE_CONTROL)
						{
							s32 index1 = (s32)s_editGuidelines.guidelines.vtx.size();
							s32 index0 = s_editGuidelines.guidelines.edge.back().idx[1];
							s_editGuidelines.guidelines.edge.push_back({ index0, index1, -1 });
							s_editGuidelines.guidelines.vtx.push_back(onGrid);
						}
						s_editGuidelines.shapeComplete = true;
					}
					else
					{
						// Make sure the vertex hasn't already been placed.
						const s32 count = (s32)s_editGuidelines.guidelines.vtx.size();
						const Vec2f* vtx = s_editGuidelines.guidelines.vtx.data();
						bool vtxExists = false;
						for (s32 i = 0; i < count; i++, vtx++)
						{
							if (TFE_Polygon::vtxEqual(vtx, &onGrid))
							{
								vtxExists = true;
								break;
							}
						}
						if (!vtxExists)
						{
							s32 index1 = (s32)s_editGuidelines.guidelines.vtx.size();
							s32 index0 = s_editGuidelines.guidelines.edge.empty() ? 0 : s_editGuidelines.guidelines.edge.back().idx[1];
							s_editGuidelines.guidelines.edge.push_back({ index0, index1, -1 });
							s_editGuidelines.guidelines.vtx.push_back(onGrid);
						}
					}
				}
				else if (s_drawActions & DRAW_ACTION_CANCEL)
				{
					removeLastEdge();
				}
				else if ((s_drawActions & DRAW_ACTION_FINISH) || s_doubleClick)
				{
					createGuidelinesFromShape();
				}
			}
			else if (s_editGuidelines.drawMode == DMODE_CURVE_CONTROL)
			{
				s_editGuidelines.controlPoint = onGrid;

				// Click to finalize the curve.
				if (s_drawActions & DRAW_ACTION_CANCEL)
				{
					s_editGuidelines.drawMode = DMODE_SHAPE;
					removeLastEdge();
				}
				else if ((s_drawActions & DRAW_ACTION_FINISH) || s_doubleClick)
				{
					s32 cindex = (s32)s_editGuidelines.guidelines.vtx.size();
					s_editGuidelines.guidelines.vtx.push_back(s_editGuidelines.controlPoint);
					s_editGuidelines.guidelines.edge.back().idx[2] = cindex;

					createGuidelinesFromShape();
				}
				else if (s_singleClick)
				{
					if (s_editGuidelines.shapeComplete)
					{
						// Remove the last vertex, since it is a duplicate of the first.
						s_editGuidelines.guidelines.vtx.pop_back();
						s_editGuidelines.guidelines.edge.back().idx[1] = 0;
					}

					s32 cindex = (s32)s_editGuidelines.guidelines.vtx.size();
					s_editGuidelines.guidelines.vtx.push_back(s_editGuidelines.controlPoint);
					s_editGuidelines.guidelines.edge.back().idx[2] = cindex;
					// Change back to shape draw mode.
					s_editGuidelines.drawMode = DMODE_SHAPE;
					// If the last point and first point is the same, the shape is complete and we're done drawing it.
					if (s_editGuidelines.shapeComplete)
					{
						// Finish.
						createGuidelinesFromShape();
					}
				}
			}
		}
		else if (s_singleClick)
		{
			if (TFE_Input::keyModDown(KeyModifier::KEYMOD_CTRL))
			{
				s_editGuidelines.drawStarted = false;
				selectGuideline();
			}
			else
			{
				s_editGuidelines.drawStarted = true;
				s_editGuidelines.shapeComplete = false;
				s_editGuidelines.drawMode = TFE_Input::keyModDown(KEYMOD_SHIFT) ? DMODE_RECT : DMODE_SHAPE;
				s_editGuidelines.drawCurPos = onGrid;

				s_editGuidelines.guidelines.vtx.clear();
				s_editGuidelines.guidelines.edge.clear();
				if (s_editGuidelines.drawMode == DMODE_RECT)
				{
					s_editGuidelines.guidelines.vtx.resize(2);
					s_editGuidelines.guidelines.vtx[0] = onGrid;
					s_editGuidelines.guidelines.vtx[1] = onGrid;
				}
				else
				{
					s_editGuidelines.guidelines.vtx.push_back(onGrid);
				}
			}
		}
		else
		{
			s_editGuidelines.drawCurPos = onGrid;
		}
	}

	void findHoveredGuideline()
	{
		const s32 count = (s32)s_level.guidelines.size();
		const Guideline* guideline = s_level.guidelines.data();
		for (s32 i = 0; i < count; i++, guideline++)
		{
			if (s_cursor3d.x < guideline->bounds.x || s_cursor3d.x > guideline->bounds.z ||
				s_cursor3d.z < guideline->bounds.y || s_cursor3d.z > guideline->bounds.w)
			{
				continue;
			}

			// for now just accept it.
			// TODO: Check signed distance and make sure it is close enough (based on zoom).
			s_hoveredGuideline = i;
			break;
		}
	}

	void selectGuideline()
	{
		s_curGuideline = s_hoveredGuideline;
	}

	void setDefaultGuidelineValues(Guideline& guideline)
	{
		guideline.maxSnapRange = s_grid.size;
		guideline.minHeight = s_grid.height;
		guideline.maxHeight = s_grid.height + 16.0f;
		guideline.maxOffset = 0.0f;
	}

	void createGuidlinesFromRect()
	{
		s_editGuidelines.drawStarted = false;
		s_editGuidelines.shapeComplete = false;

		Vec2f p[4];
		Vec2f* srcVtx = s_editGuidelines.guidelines.vtx.data();
		getGridOrientedRect(srcVtx[0], srcVtx[1], p);

		Guideline guideline;
		guideline.vtx.resize(4);
		guideline.edge.resize(4);

		guideline.vtx[0] = p[0];
		guideline.vtx[1] = p[1];
		guideline.vtx[2] = p[2];
		guideline.vtx[3] = p[3];

		guideline.edge.resize(4);
		for (s32 e = 0; e < 4; e++)
		{
			guideline.edge[e].idx[0] = e;
			guideline.edge[e].idx[1] = (e + 1)&3;
			guideline.edge[e].idx[2] = -1;
		}

		setDefaultGuidelineValues(guideline);
		guideline_computeBounds(&guideline);
		guideline_computeKnots(&guideline);
		guideline.id = (s32)s_level.guidelines.size();
		s_level.guidelines.push_back(guideline);

		cmd_guidelineSnapshot(LName_Guideline_Create);
	}

	void createGuidelinesFromShape()
	{
		s_editGuidelines.drawStarted = false;
		s_editGuidelines.shapeComplete = false;

		Guideline& guideline = s_editGuidelines.guidelines;
		if (guideline.vtx.size() < 2 || guideline.edge.empty())
		{
			return;
		}

		setDefaultGuidelineValues(guideline);
		guideline_computeBounds(&guideline);
		guideline_computeKnots(&guideline);
		guideline.id = (s32)s_level.guidelines.size();
		s_level.guidelines.push_back(guideline);

		cmd_guidelineSnapshot(LName_Guideline_Create);
	}
}
