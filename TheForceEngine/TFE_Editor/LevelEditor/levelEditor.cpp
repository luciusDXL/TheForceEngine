#include "levelEditor.h"
#include "levelEditorData.h"
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Editor/LevelEditor/Rendering/viewport.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editorLevel.h>
#include <TFE_Editor/editorProject.h>
#include <TFE_Editor/editorResources.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	const f32 c_defaultZoom = 0.25f;

	// The TFE Level Editor format is different than the base format and contains extra 
	// metadata, etc.
	// So existing levels need to be loaded into that format.
	// If the correct format already exists, though, then it is loaded directly.
	EditorLevel s_level = {};
	LevelEditMode s_editMode = LEDIT_DRAW;
	u32 s_editFlags = LEF_DEFAULT;
	s32 s_curLayer = 0;
		
	// Sector
	EditorSector* s_hoveredSector = nullptr;
	EditorSector* s_selectedSector = nullptr;

	// Vertex
	EditorSector* s_hoveredVtxSector = nullptr;
	EditorSector* s_lastHoveredVtxSector = nullptr;
	EditorSector* s_selectedVtxSector = nullptr;
	s32 s_hoveredVtxId = -1;
	s32 s_selectedVtxId = -1;

	// Wall
	EditorSector* s_hoveredWallSector = nullptr;
	EditorSector* s_lastHoveredWallSector = nullptr;
	EditorSector* s_selectedWallSector = nullptr;
	s32 s_hoveredWallId = -1;
	s32 s_selectedWallId = -1;
		
	static EditorView s_view = EDIT_VIEW_2D;
	static Vec2i s_editWinPos = { 0, 69 };
	static Vec2i s_editWinSize = { 0 };
	static Vec2f s_editWinMapCorner = { 0 };
	static f32 s_gridSize = 32.0f;
	static f32 s_zoom = c_defaultZoom;

	static AssetList s_textureList;

	static char s_layerMem[4 * 31];
	static char* s_layerStr[31];
		
	static s32 s_gridIndex = 8;
	static f32 c_gridSizeMap[] =
	{
		0.015625f, 0.03125f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f
	};
	static const char* c_gridSizes[] =
	{
		"1/64",
		"1/32",
		"1/16",
		"1/8",
		"1/4",
		"1/2",
		"1",
		"2",
		"4",
		"8",
		"16",
		"32",
		"64",
	};
	
	static TextureGpu* s_editCtrlToolbarData = nullptr;

	////////////////////////////////////////////////////////
	// Forward Declarations
	////////////////////////////////////////////////////////
	void toolbarBegin();
	void toolbarEnd();
	bool drawToolbarButton(void* ptr, u32 imageId, bool highlight);
	void levelEditWinBegin();
	void levelEditWinEnd();
	void messagePanel(ImVec2 pos);
	void cameraControl2d(s32 mx, s32 my);
	void resetZoom();
	Vec2f mouseCoordToWorldPos2d(s32 mx, s32 my);
	Vec2i worldPos2dToMap(const Vec2f& worldPos);
	
	////////////////////////////////////////////////////////
	// Public API
	////////////////////////////////////////////////////////
	TextureGpu* loadGpuImage(const char* path)
	{
		char imagePath[TFE_MAX_PATH];
		strcpy(imagePath, path);
		if (!TFE_Paths::mapSystemPath(imagePath))
		{
			memset(imagePath, 0, TFE_MAX_PATH);
			TFE_Paths::appendPath(TFE_PathType::PATH_PROGRAM, path, imagePath, TFE_MAX_PATH);
			FileUtil::fixupPath(imagePath);
		}

		TextureGpu* gpuImage = nullptr;
		SDL_Surface* image = TFE_Image::get(path);
		if (image)
		{
			gpuImage = TFE_RenderBackend::createTexture(image->w, image->h, (u32*)image->pixels, MAG_FILTER_LINEAR);
		}
		return gpuImage;
	}

	bool init(Asset* asset)
	{
		// Cleanup any existing level data.
		destroy();
		// Load the new level.
		if (!loadLevelFromAsset(asset, &s_level))
		{
			return false;
		}
		viewport_init();
		viewport_update((s32)UI_SCALE(480) + 16, (s32)UI_SCALE(68) + 18);
		s_gridIndex = 8;
		s_gridSize = 4.0f;
		s_gridSize2d = s_gridSize;

		s_editCtrlToolbarData = loadGpuImage("UI_Images/EditCtrl_32x6.png");

		u32 idx = 0;
		for (s32 i = -15; i < 16; i++, idx += 4)
		{
			s_layerStr[i + 15] = &s_layerMem[idx];
			sprintf(s_layerStr[i + 15], "%d", i);
		}

		s_viewportPos = { -24.0f, 0.0f, -200.0f };
		s_curLayer = std::min(1, s_level.layerRange[1]);

		s_sectorDrawMode = SDM_WIREFRAME;
		s_hoveredSector         = nullptr;
		s_selectedSector        = nullptr;
		s_hoveredVtxSector      = nullptr;
		s_selectedVtxSector     = nullptr;
		s_lastHoveredVtxSector  = nullptr;
		s_hoveredWallSector     = nullptr;
		s_lastHoveredWallSector = nullptr;
		s_selectedWallSector    = nullptr;
		s_hoveredVtxId          = -1;
		s_hoveredWallId         = -1;

		AssetBrowser::getLevelTextures(s_textureList, asset->name.c_str());

		TFE_RenderShared::init(false);
		return true;
	}

	void destroy()
	{
		s_level.sectors.clear();
		viewport_destroy();
		TFE_RenderShared::destroy();

		TFE_RenderBackend::freeTexture(s_editCtrlToolbarData);
		s_editCtrlToolbarData = nullptr;
	}

	bool isPointInsideSector2d(EditorSector* sector, Vec2f pos, s32 layer)
	{
		// The layers need to match.
		if (sector->layer != layer) { return false; }
		// The point has to be within the bounding box.
		if (pos.x < sector->bounds[0].x || pos.x > sector->bounds[1].x ||
			pos.z < sector->bounds[0].z || pos.z > sector->bounds[1].z)
		{
			return false;
		}
		return TFE_Polygon::pointInsidePolygon(&sector->poly, pos);
	}

	EditorSector* findSector2d(Vec2f pos, s32 layer)
	{
		const size_t sectorCount = s_level.sectors.size();
		EditorSector* sector = s_level.sectors.data();
		for (size_t s = 0; s < sectorCount; s++, sector++)
		{
			if (isPointInsideSector2d(sector, pos, layer))
			{
				return sector;
			}
		}
		return nullptr;
	}

	// Find the closest point to p2 on line segment p0 -> p1 as a parametric value on the segment.
	// Fills in point with the point itself.
	f32 closestPointOnLineSegment(Vec2f p0, Vec2f p1, Vec2f p2, Vec2f* point)
	{
		const Vec2f r = { p2.x - p0.x, p2.z - p0.z };
		const Vec2f d = { p1.x - p0.x, p1.z - p0.z };
		const f32 denom = d.x * d.x + d.z * d.z;
		if (fabsf(denom) < FLT_EPSILON) { return 0.0f; }

		const f32 s = std::max(0.0f, std::min(1.0f, (r.x * d.x + r.z * d.z) / denom));
		point->x = p0.x + s * d.x;
		point->z = p0.z + s * d.z;
		return s;
	}

	s32 findClosestWallInSector(const EditorSector* sector, const Vec2f* pos, f32 maxDistSq, f32* minDistToWallSq)
	{
		const u32 count = (u32)sector->walls.size();
		f32 minDistSq = FLT_MAX;
		s32 closestId = -1;
		const EditorWall* walls = sector->walls.data();
		const Vec2f* vertices = sector->vtx.data();
		for (u32 w = 0; w < count; w++)
		{
			const Vec2f* v0 = &vertices[walls[w].idx[0]];
			const Vec2f* v1 = &vertices[walls[w].idx[1]];

			Vec2f pointOnSeg;
			closestPointOnLineSegment(*v0, *v1, *pos, &pointOnSeg);
			const Vec2f diff = { pointOnSeg.x - pos->x, pointOnSeg.z - pos->z };
			const f32 distSq = diff.x*diff.x + diff.z*diff.z;

			if (distSq < maxDistSq && distSq < minDistSq && (!minDistToWallSq || distSq < *minDistToWallSq))
			{
				minDistSq = distSq;
				closestId = s32(w);
			}
		}
		if (minDistToWallSq)
		{
			*minDistToWallSq = std::min(*minDistToWallSq, minDistSq);
		}
		return closestId;
	}
		
	void updateWindowControls()
	{
		if (getMenuActive()) { return; }

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (!TFE_Input::relativeModeEnabled() && (mx < s_editWinPos.x || mx >= s_editWinPos.x + s_editWinSize.x || my < s_editWinPos.z || my >= s_editWinPos.z + s_editWinSize.z))
		{
			// Nothing is "hovered" if the mouse is not in the window.
			s_hoveredSector = nullptr;
			s_hoveredVtxSector = nullptr;
			s_lastHoveredVtxSector = nullptr;
			s_hoveredWallSector = nullptr;
			s_lastHoveredWallSector = nullptr;
			s_hoveredVtxId = -1;
			s_hoveredWallId = -1;
			return;
		}

		if (s_view == EDIT_VIEW_2D)
		{
			cameraControl2d(mx, my);

			// Selection
			Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
			if (s_editMode != LEDIT_DRAW)
			{
				// First check to see if the current hovered sector is still valid.
				if (s_hoveredSector)
				{
					if (!isPointInsideSector2d(s_hoveredSector, worldPos, s_curLayer))
					{
						s_hoveredSector = nullptr;
					}
				}
				// If not, then try to find one.
				if (!s_hoveredSector)
				{
					s_hoveredSector = findSector2d(worldPos, s_curLayer);
				}
				
				if (s_editMode == LEDIT_SECTOR && TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
				{
					s_selectedSector = s_hoveredSector;
				}
				else if (s_editMode != LEDIT_SECTOR)
				{
					s_selectedSector = nullptr;
				}

				if (s_editMode == LEDIT_VERTEX)
				{
					// See if we are close enough to "hover" a vertex
					s_hoveredVtxSector = nullptr;
					s_hoveredVtxId = -1;
					if (s_hoveredSector || s_lastHoveredVtxSector)
					{
						// Keep track of the last vertex hovered sector and use it if no hovered sector is active to
						// make selecting vertices less fiddly.
						EditorSector* hoveredSector = s_hoveredSector ? s_hoveredSector : s_lastHoveredVtxSector;

						const size_t vtxCount = hoveredSector->vtx.size();
						const Vec2f* vtx = hoveredSector->vtx.data();

						f32 closestDistSq = FLT_MAX;
						s32 closestVtx = -1;
						for (size_t v = 0; v < vtxCount; v++, vtx++)
						{
							Vec2f offset = { worldPos.x - vtx->x, worldPos.z - vtx->z };
							f32 distSq = offset.x*offset.x + offset.z*offset.z;
							if (distSq < closestDistSq)
							{
								closestDistSq = distSq;
								closestVtx = (s32)v;
							}
						}

						const f32 maxDist = s_zoom2d * 16.0f;
						if (closestDistSq <= maxDist*maxDist)
						{
							s_hoveredVtxSector = hoveredSector;
							s_lastHoveredVtxSector = hoveredSector;
							s_hoveredVtxId = closestVtx;
						}
					}

					if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
					{
						s_selectedVtxSector = nullptr;
						s_selectedVtxId = -1;
						if (s_hoveredVtxSector && s_hoveredVtxId >= 0)
						{
							s_selectedVtxSector = s_hoveredVtxSector;
							s_selectedVtxId = s_hoveredVtxId;
						}
					}
				}
				else
				{
					s_hoveredVtxSector = nullptr;
					s_selectedVtxSector = nullptr;
					s_hoveredVtxId = -1;
					s_selectedVtxId = -1;
				}

				if (s_editMode == LEDIT_WALL)
				{
					// See if we are close enough to "hover" a vertex
					s_hoveredWallSector = nullptr;
					s_hoveredWallId = -1;
					if (s_hoveredSector || s_lastHoveredWallSector)
					{
						// Keep track of the last vertex hovered sector and use it if no hovered sector is active to
						// make selecting vertices less fiddly.
						EditorSector* hoveredSector = s_hoveredSector ? s_hoveredSector : s_lastHoveredWallSector;

						const f32 maxDist = s_zoom2d * 16.0f;
						s_hoveredWallId = findClosestWallInSector(hoveredSector, &worldPos, maxDist * maxDist, nullptr);
						if (s_hoveredWallId >= 0)
						{
							s_hoveredWallSector = hoveredSector;
							s_lastHoveredWallSector = hoveredSector;
						}
						else
						{
							s_hoveredWallSector = nullptr;
						}
					}

					if (TFE_Input::mousePressed(MouseButton::MBUTTON_LEFT))
					{
						s_selectedWallSector = nullptr;
						s_selectedWallId = -1;
						if (s_hoveredWallSector && s_hoveredWallId >= 0)
						{
							s_selectedWallSector = s_hoveredWallSector;
							s_selectedWallId = s_hoveredWallId;
						}
					}
				}
				else
				{
					s_hoveredWallSector = nullptr;
					s_selectedWallSector = nullptr;
					s_hoveredWallId = -1;
					s_selectedWallId = -1;
				}

				// DEBUG
				if (s_selectedSector && TFE_Input::keyPressed(KEY_T))
				{
					TFE_Polygon::computeTriangulation(&s_selectedSector->poly);
				}
			}
		}
	}

	bool menu()
	{
		bool menuActive = false;
		if (ImGui::BeginMenu("Level"))
		{
			menuActive = true;
			// Disable Save/Backup when there is no project.
			bool projectActive = project_get()->active;
			if (!projectActive) { disableNextItem(); }
			if (ImGui::MenuItem("Save", "Ctrl+S", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Backup", "Ctrl+B", (bool*)NULL))
			{
			}
			if (!projectActive) { enableNextItem(); }

			if (ImGui::MenuItem("Close", NULL, (bool*)NULL))
			{
				// TODO: If the level has changed, pop up a warning and allow the level to be saved.
				disableAssetEditor();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Undo", "Ctrl+Z", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", (bool*)NULL))
			{
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Copy", "Ctrl+C", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Paste", "Ctrl+V", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Delete", "Del", (bool*)NULL))
			{
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			menuActive = true;
			if (ImGui::MenuItem("2D", "Ctrl+1", s_view == EDIT_VIEW_2D))
			{
				s_view = EDIT_VIEW_2D;
			}
			if (ImGui::MenuItem("3D (Editor)", "Ctrl+2", s_view == EDIT_VIEW_3D))
			{
				// TODO
			}
			if (ImGui::MenuItem("3D (Game)", "Ctrl+3", s_view == EDIT_VIEW_3D_GAME))
			{
				// TODO
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Zoom", nullptr, (bool*)NULL))
			{
				resetZoom();
			}

			bool showLower = (s_editFlags & LEF_SHOW_LOWER_LAYERS) != 0;
			if (ImGui::MenuItem("Show Lower Layers", "Ctrl+L", showLower))
			{
				showLower = !showLower;
				if (showLower) { s_editFlags |= LEF_SHOW_LOWER_LAYERS;  }
				else { s_editFlags &= ~LEF_SHOW_LOWER_LAYERS; }
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Wireframe", "Ctrl+F1", s_sectorDrawMode == SDM_WIREFRAME))
			{
				s_sectorDrawMode = SDM_WIREFRAME;
			}
			if (ImGui::MenuItem("Lighting", "Ctrl+F2", s_sectorDrawMode == SDM_LIGHTING))
			{
				s_sectorDrawMode = SDM_LIGHTING;
			}
			if (ImGui::MenuItem("Textured (Floor)", "Ctrl+F3", s_sectorDrawMode == SDM_TEXTURED_FLOOR))
			{
				s_sectorDrawMode = SDM_TEXTURED_FLOOR;
			}
			if (ImGui::MenuItem("Textured (Ceiling)", "Ctrl+F4", s_sectorDrawMode == SDM_TEXTURED_CEIL))
			{
				s_sectorDrawMode = SDM_TEXTURED_CEIL;
			}
			ImGui::Separator();
			bool fullbright = (s_editFlags & LEF_FULLBRIGHT) != 0;
			if (ImGui::MenuItem("Fullbright", "Ctrl+F5", fullbright))
			{
				fullbright = !fullbright;
				if (fullbright) { s_editFlags |= LEF_FULLBRIGHT; }
				else { s_editFlags &= ~LEF_FULLBRIGHT; }
			}
			ImGui::EndMenu();
		}

		return menuActive;
	}

	static s32 s_infoHeight;
	void infoToolBegin(s32 height);
	void infoToolEnd();
	void drawInfoPanel();

	void infoToolBegin(s32 height)
	{
		bool infoTool = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Info Panel", { (f32)displayInfo.width - 480.0f, 22.0f });
		ImGui::SetWindowSize("Info Panel", { 480.0f, f32(height) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Info Panel", &infoTool, window_flags); ImGui::SameLine();
	}

	void infoPanelMap()
	{
		char name[64];
		strcpy(name, s_level.name.c_str());

		ImGui::Text("Level Name:"); ImGui::SameLine();
		if (ImGui::InputText("", name, 64))
		{
			s_level.name = name;
		}
		ImGui::Text("Sector Count: %u", (u32)s_level.sectors.size());
		ImGui::Text("Bounds: (%0.3f, %0.3f, %0.3f)\n        (%0.3f, %0.3f, %0.3f)", s_level.bounds[0].x, s_level.bounds[0].y, s_level.bounds[0].z,
			s_level.bounds[1].x, s_level.bounds[1].y, s_level.bounds[1].z);
		ImGui::Text("Layer Range: [%d, %d]", s_level.layerRange[0], s_level.layerRange[1]);
		ImGui::Separator();
		//ImGui::LabelText("##GridLabel", "Grid Height");
		//ImGui::SetNextItemWidth(196.0f);
		//ImGui::InputFloat("##GridHeight", &s_gridHeight, 0.0f, 0.0f, "%0.2f", ImGuiInputTextFlags_CharsDecimal);
		//ImGui::Checkbox("Grid Auto Adjust", &s_gridAutoAdjust);
		//ImGui::Checkbox("Show Grid When Camera Is Inside a Sector", &s_showGridInSector);
	}

	void infoPanelVertex()
	{
		EditorSector* sector = (s_selectedVtxId >= 0) ? s_selectedVtxSector : s_hoveredVtxSector;
		s32 index = s_selectedVtxId >= 0 ? s_selectedVtxId : s_hoveredVtxId;
		if (index < 0 || !sector) { return; }

		Vec2f* vtx = sector->vtx.data() + index;

		ImGui::Text("Vertex %d of Sector %d", index, sector->id);

		ImGui::NewLine();
		ImGui::PushItemWidth(UI_SCALE(80));
		ImGui::LabelText("##PositionLabel", "Position");
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::PushItemWidth(196.0f);
		ImGui::InputFloat2("##VertexPosition", &vtx->x, "%0.3f", ImGuiInputTextFlags_CharsDecimal);
		ImGui::PopItemWidth();
	}

	void infoLabel(const char* labelId, const char* labelText, u32 width)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::LabelText(labelId, labelText);
		ImGui::PopItemWidth();
		ImGui::SameLine();
	}

	void infoIntInput(const char* labelId, u32 width, s32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputInt(labelId, value);
		ImGui::PopItemWidth();
	}

	void infoUIntInput(const char* labelId, u32 width, u32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputUInt(labelId, value);
		ImGui::PopItemWidth();
	}

	void infoFloatInput(const char* labelId, u32 width, f32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputFloat(labelId, value, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
	}

	void infoPanelWall()
	{
		s32 wallId;
		EditorSector* sector;
		if (s_selectedWallId >= 0) { sector = s_selectedWallSector; wallId = s_selectedWallId; }
		else if (s_hoveredWallId >= 0) { sector = s_hoveredWallSector; wallId = s_hoveredWallId; }
		else { return; }

		EditorWall* wall = sector->walls.data() + wallId;
		Vec2f* vertices = sector->vtx.data();
		f32 len = TFE_Math::distance(&vertices[wall->idx[0]], &vertices[wall->idx[1]]);

		ImGui::Text("Wall ID: %d  Sector ID: %d  Length: %2.2f", wallId, sector->id, len);
		ImGui::Text("Vertices: [%d](%2.2f, %2.2f), [%d](%2.2f, %2.2f)", wall->idx[0], vertices[wall->idx[0]].x, vertices[wall->idx[0]].z, wall->idx[1], vertices[wall->idx[1]].x, vertices[wall->idx[1]].z);
		ImGui::Separator();

		// Adjoin data (should be carefully and rarely edited directly).
		ImGui::LabelText("##SectorAdjoin", "Adjoin"); ImGui::SameLine(64.0f);
		infoIntInput("##SectorAdjoinInput", 96, &wall->adjoinId); ImGui::SameLine(180.0f);

		ImGui::LabelText("##SectorMirror", "Mirror"); ImGui::SameLine(240);
		infoIntInput("##SectorMirrorInput", 96, &wall->mirrorId);

		s32 light = wall->wallLight;
		ImGui::LabelText("##SectorLight", "Light Adjustment"); ImGui::SameLine(148.0f);
		infoIntInput("##SectorLightInput", 96, &light);
		wall->wallLight = (s16)light;

		ImGui::Separator();

		// Flags
		infoLabel("##Flags1Label", "Flags1", 48);
		infoUIntInput("##Flags1", 128, &wall->flags[0]);
		ImGui::SameLine();

		infoLabel("##Flags3Label", "Flags3", 48);
		infoUIntInput("##Flags3", 128, &wall->flags[2]);

		const f32 column[] = { 0.0f, 174.0f, 324.0f };

		ImGui::CheckboxFlags("Mask Wall##WallFlag", &wall->flags[0], WF1_ADJ_MID_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Illum Sign##WallFlag", &wall->flags[0], WF1_ILLUM_SIGN); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Horz Flip Tex##SectorFlag", &wall->flags[0], WF1_FLIP_HORIZ);

		ImGui::CheckboxFlags("Change WallLight##WallFlag", &wall->flags[0], WF1_CHANGE_WALL_LIGHT); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Tex Anchored##WallFlag", &wall->flags[0], WF1_TEX_ANCHORED); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Wall Morphs##SectorFlag", &wall->flags[0], WF1_WALL_MORPHS);

		ImGui::CheckboxFlags("Scroll Top Tex##WallFlag", &wall->flags[0], WF1_SCROLL_TOP_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Scroll Mid Tex##WallFlag", &wall->flags[0], WF1_SCROLL_MID_TEX); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Scroll Bottom##SectorFlag", &wall->flags[0], WF1_SCROLL_BOT_TEX);

		ImGui::CheckboxFlags("Scroll Sign##WallFlag", &wall->flags[0], WF1_SCROLL_SIGN_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Hide On Map##WallFlag", &wall->flags[0], WF1_HIDE_ON_MAP); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Show On Map##SectorFlag", &wall->flags[0], WF1_SHOW_NORMAL_ON_MAP);

		ImGui::CheckboxFlags("Sign Anchored##WallFlag", &wall->flags[0], WF1_SIGN_ANCHORED); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Damage Wall##WallFlag", &wall->flags[0], WF1_DAMAGE_WALL); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Show As Ledge##SectorFlag", &wall->flags[0], WF1_SHOW_AS_LEDGE_ON_MAP);

		ImGui::CheckboxFlags("Show As Door##WallFlag", &wall->flags[0], WF1_SHOW_AS_DOOR_ON_MAP); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Always Walk##WallFlag", &wall->flags[2], WF3_ALWAYS_WALK); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Solid Wall##SectorFlag", &wall->flags[2], WF3_SOLID_WALL);

		ImGui::CheckboxFlags("Player Walk Only##WallFlag", &wall->flags[2], WF3_PLAYER_WALK_ONLY); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Cannot Shoot Thru##WallFlag", &wall->flags[2], WF3_CANNOT_FIRE_THROUGH);

		ImGui::Separator();

		EditorTexture* midTex = (EditorTexture*)getAssetData(wall->tex[WP_MID].handle);
		EditorTexture* topTex = (EditorTexture*)getAssetData(wall->tex[WP_TOP].handle);
		EditorTexture* botTex = (EditorTexture*)getAssetData(wall->tex[WP_BOT].handle);
		EditorTexture* sgnTex = (EditorTexture*)getAssetData(wall->tex[WP_SIGN].handle);

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Mid Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Sign Texture");

		// Textures - Mid / Sign
		const f32 midScale = midTex ? 1.0f / (f32)std::max(midTex->width, midTex->height) : 0.0f;
		const f32 sgnScale = sgnTex ? 1.0f / (f32)std::max(sgnTex->width, sgnTex->height) : 0.0f;
		const f32 aspectMid[] = { midTex ? f32(midTex->width) * midScale : 1.0f, midTex ? f32(midTex->height) * midScale : 1.0f };
		const f32 aspectSgn[] = { sgnTex ? f32(sgnTex->width) * sgnScale : 1.0f, sgnTex ? f32(sgnTex->height) * sgnScale : 1.0f };

		ImGui::ImageButton(midTex ? TFE_RenderBackend::getGpuPtr(midTex->frames[0]) : nullptr, { 128.0f * aspectMid[0], 128.0f * aspectMid[1] });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(sgnTex ? TFE_RenderBackend::getGpuPtr(sgnTex->frames[0]) : nullptr, { 128.0f * aspectSgn[0], 128.0f * aspectSgn[1] });
		const ImVec2 imageLeft0 = ImGui::GetItemRectMin();
		const ImVec2 imageRight0 = ImGui::GetItemRectMax();

		// Names
		ImGui::Text("%s %dx%d", midTex ? midTex->name : "NONE", midTex ? midTex->width : 0, midTex ? midTex->height : 0); ImGui::SameLine(texCol);
		ImGui::Text("%s %dx%d", sgnTex ? sgnTex->name : "NONE", sgnTex ? sgnTex->width : 0, sgnTex ? sgnTex->height : 0);

		ImVec2 imageLeft1, imageRight1;
		if (wall->adjoinId >= 0)
		{
			ImGui::NewLine();

			// Textures - Top / Bottom
			// Labels
			ImGui::Text("Top Texture"); ImGui::SameLine(texCol); ImGui::Text("Bottom Texture");

			const f32 topScale = topTex ? 1.0f / (f32)std::max(topTex->width, topTex->height) : 0.0f;
			const f32 botScale = botTex ? 1.0f / (f32)std::max(botTex->width, botTex->height) : 0.0f;
			const f32 aspectTop[] = { topTex ? f32(topTex->width) * topScale : 1.0f, topTex ? f32(topTex->height) * topScale : 1.0f };
			const f32 aspectBot[] = { botTex ? f32(botTex->width) * botScale : 1.0f, botTex ? f32(botTex->height) * botScale : 1.0f };

			ImGui::ImageButton(topTex ? TFE_RenderBackend::getGpuPtr(topTex->frames[0]) : nullptr, { 128.0f * aspectTop[0], 128.0f * aspectTop[1] });
			ImGui::SameLine(texCol);
			ImGui::ImageButton(botTex ? TFE_RenderBackend::getGpuPtr(botTex->frames[0]) : nullptr, { 128.0f * aspectBot[0], 128.0f * aspectBot[1] });
			imageLeft1 = ImGui::GetItemRectMin();
			imageRight1 = ImGui::GetItemRectMax();

			// Names
			ImGui::Text("%s %dx%d", topTex ? topTex->name : "NONE", topTex ? topTex->width : 0, topTex ? topTex->height : 0); ImGui::SameLine(texCol);
			ImGui::Text("%s %dx%d", botTex ? botTex->name : "NONE", botTex ? botTex->width : 0, botTex ? botTex->height : 0);
		}

		// Texture Offsets
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		// Set 0
		ImVec2 offsetLeft = { imageLeft0.x + texCol + 8.0f, imageLeft0.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft0.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsets0Wall", offsetSize);
		{
			ImGui::Text("Mid Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##MidOffsetInput", &wall->tex[WP_MID].offset.x, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Sign Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##SignOffsetInput", &wall->tex[WP_SIGN].offset.x, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();

		// Set 1
		if (wall->adjoinId >= 0)
		{
			offsetLeft = { imageLeft1.x + texCol + 8.0f, imageLeft1.y + 8.0f };
			offsetSize = { displayInfo.width - (imageLeft1.x + texCol + 16.0f), 128.0f };
			ImGui::SetNextWindowPos(offsetLeft);
			// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
			ImGui::BeginChild("##TextureOffsets1Wall", offsetSize);
			{
				ImGui::Text("Top Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##TopOffsetInput", &wall->tex[WP_TOP].offset.x, "%.2f");
				ImGui::PopItemWidth();

				ImGui::NewLine();

				ImGui::Text("Bottom Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##BotOffsetInput", &wall->tex[WP_BOT].offset.x, "%.2f");
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
	}

	void infoPanelSector()
	{
		EditorSector* sector = s_selectedSector ? s_selectedSector : s_hoveredSector;
		ImGui::Text("Sector ID: %d      Wall Count: %u", sector->id, (u32)sector->walls.size());
		ImGui::Separator();

		// Sector Name (optional, used by INF)
		char sectorName[64];
		strcpy(sectorName, sector->name.c_str());

		infoLabel("##NameLabel", "Name", 32);
		ImGui::PushItemWidth(240.0f);
		if (ImGui::InputText("##NameSector", sectorName, 64))
		{
			sector->name = sectorName;
		}
		ImGui::PopItemWidth();

		// Layer and Ambient
		s32 layer = sector->layer;
		infoLabel("##LayerLabel", "Layer", 42);
		infoIntInput("##LayerSector", 96, &layer);
		if (layer != sector->layer)
		{
			sector->layer = layer;
			// Adjust layer range.
			s_level.layerRange[0] = std::min(s_level.layerRange[0], (s32)layer);
			s_level.layerRange[1] = std::max(s_level.layerRange[1], (s32)layer);
			// Change the current layer so we can still see the sector.
			s_curLayer = layer;
		}

		ImGui::SameLine(0.0f, 16.0f);

		s32 ambient = (s32)sector->ambient;
		infoLabel("##AmbientLabel", "Ambient", 56);
		infoIntInput("##AmbientSector", 96, &ambient);
		sector->ambient = std::max(0, std::min(31, ambient));

		// Heights
		infoLabel("##FloorHeightLabel", "Floor Ht", 66);
		infoFloatInput("##FloorHeight", 64, &sector->floorHeight);
		ImGui::SameLine();

		infoLabel("##SecondHeightLabel", "Second Ht", 72);
		infoFloatInput("##SecondHeight", 64, &sector->secHeight);
		ImGui::SameLine();

		infoLabel("##CeilHeightLabel", "Ceiling Ht", 80);
		infoFloatInput("##CeilHeight", 64, &sector->ceilHeight);

		ImGui::Separator();

		// Flags
		infoLabel("##Flags1Label", "Flags1", 48);
		infoUIntInput("##Flags1", 128, &sector->flags[0]);
		ImGui::SameLine();

		infoLabel("##Flags2Label", "Flags2", 48);
		infoUIntInput("##Flags2", 128, &sector->flags[1]);

		infoLabel("##Flags3Label", "Flags3", 48);
		infoUIntInput("##Flags3", 128, &sector->flags[2]);

		const f32 column[] = { 0.0f, 160.0f, 320.0f };

		ImGui::CheckboxFlags("Exterior##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXTERIOR); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Pit##SectorFlag", &sector->flags[0], SEC_FLAGS1_PIT); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("No Walls##SectorFlag", &sector->flags[0], SEC_FLAGS1_NOWALL_DRAW);

		ImGui::CheckboxFlags("Ext Ceil Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_ADJ); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Ext Floor Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_FLOOR_ADJ);  ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Secret##SectorFlag", &sector->flags[0], SEC_FLAGS1_SECRET);

		ImGui::CheckboxFlags("Door##SectorFlag", &sector->flags[0], SEC_FLAGS1_DOOR); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Ice Floor##SectorFlag", &sector->flags[0], SEC_FLAGS1_ICE_FLOOR); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Snow Floor##SectorFlag", &sector->flags[0], SEC_FLAGS1_SNOW_FLOOR);

		ImGui::CheckboxFlags("Crushing##SectorFlag", &sector->flags[0], SEC_FLAGS1_CRUSHING); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Low Damage##SectorFlag", &sector->flags[0], SEC_FLAGS1_LOW_DAMAGE); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("High Damage##SectorFlag", &sector->flags[0], SEC_FLAGS1_HIGH_DAMAGE);

		ImGui::CheckboxFlags("No Smart Obj##SectorFlag", &sector->flags[0], SEC_FLAGS1_NO_SMART_OBJ); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Smart Obj##SectorFlag", &sector->flags[0], SEC_FLAGS1_SMART_OBJ); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Safe Sector##SectorFlag", &sector->flags[0], SEC_FLAGS1_SAFESECTOR);

		ImGui::CheckboxFlags("Mag Seal##SectorFlag", &sector->flags[0], SEC_FLAGS1_MAG_SEAL); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Exploding Wall##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXP_WALL);

		ImGui::Separator();

		// Textures
		EditorTexture* floorTex = (EditorTexture*)getAssetData(sector->floorTex.handle);
		EditorTexture* ceilTex  = (EditorTexture*)getAssetData(sector->ceilTex.handle);

		void* floorPtr = floorTex ? TFE_RenderBackend::getGpuPtr(floorTex->frames[0]) : nullptr;
		void* ceilPtr = ceilTex ? TFE_RenderBackend::getGpuPtr(ceilTex->frames[0]) : nullptr;

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Floor Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Ceiling Texture");

		// Textures
		const f32 floorScale = floorTex ? 1.0f / (f32)std::max(floorTex->width, floorTex->height) : 1.0f;
		const f32 ceilScale = ceilTex ? 1.0f / (f32)std::max(ceilTex->width, ceilTex->height) : 1.0f;
		const f32 aspectFloor[] = { floorTex ? f32(floorTex->width) * floorScale : 1.0f, floorTex ? f32(floorTex->height) * floorScale : 1.0f };
		const f32 aspectCeil[] = { ceilTex ? f32(ceilTex->width) * ceilScale : 1.0f, ceilTex ? f32(ceilTex->height) * ceilScale : 1.0f };

		ImGui::ImageButton(floorPtr, { 128.0f * aspectFloor[0], 128.0f * aspectFloor[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(ceilPtr, { 128.0f * aspectCeil[0], 128.0f * aspectCeil[1] }, { 0.0f, 1.0f }, { 1.0f, 0.0f });
		const ImVec2 imageLeft = ImGui::GetItemRectMin();
		const ImVec2 imageRight = ImGui::GetItemRectMax();

		// Names
		if (floorTex)
		{
			ImGui::Text("%s %dx%d", floorTex->name, floorTex->width, floorTex->height); ImGui::SameLine(texCol);
		}
		if (ceilTex)
		{
			ImGui::Text("%s %dx%d", ceilTex->name, ceilTex->width, ceilTex->height); ImGui::SameLine();
		}

		// Texture Offsets
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImVec2 offsetLeft = { imageLeft.x + texCol + 8.0f, imageLeft.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsetsSector", offsetSize);
		{
			ImGui::Text("Floor Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##FloorOffsetInput", &sector->floorTex.offset.x, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Ceil Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##CeilOffsetInput", &sector->ceilTex.offset.x, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();
	}
		
	void drawInfoPanel()
	{
		// Draw the info bars.
		s_infoHeight = 486 + 44;

		infoToolBegin(s_infoHeight);
		{
			if (s_editMode == LEDIT_VERTEX && (s_hoveredVtxId >= 0 || s_selectedVtxId >= 0))
			{
				infoPanelVertex();
			}
			else if (s_editMode == LEDIT_SECTOR && (s_hoveredSector || s_selectedSector))
			{
				infoPanelSector();
			}
			else if (s_editMode == LEDIT_WALL && (s_hoveredWallId >= 0 || s_selectedWallId >= 0))
			{
				infoPanelWall();
			}
			// TODO
			//else if (s_hoveredEntity >= 0 || s_selectedEntity >= 0)
			//{
			//	infoPanelEntity();
			//}
			else
			{
				infoPanelMap();
			}
		}
		infoToolEnd();
	}
		
	void infoToolEnd()
	{
		ImGui::End();
	}

	/////////////////////////////////////
	// Browser
	/////////////////////////////////////
	void browseTextures();

	void browserBegin(s32 offset)
	{
		bool browser = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Browser", { (f32)displayInfo.width - 480.0f, 22.0f + f32(offset) });
		ImGui::SetWindowSize("Browser", { 480.0f, (f32)displayInfo.height - f32(offset + 22) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Browser", &browser, window_flags);
	}

	void browserEnd()
	{
		ImGui::End();
	}

	void drawBrowser()
	{
		browserBegin(s_infoHeight);
		browseTextures();
		browserEnd();
	}

	void browseTextures()
	{
		u32 count = (u32)s_textureList.size();
		u32 rows = count / 6;
		u32 leftOver = count - rows * 6;
		f32 y = 0.0f;
		for (u32 i = 0; i < rows; i++)
		{
			for (u32 t = 0; t < 6; t++)
			{
				EditorTexture* texture = (EditorTexture*)getAssetData(s_textureList[i * 6 + t].handle);
				void* ptr = TFE_RenderBackend::getGpuPtr(texture->frames[0]);
				u32 w = 64, h = 64;
				if (texture->width > texture->height)
				{
					w = 64;
					h = 64 * texture->height / texture->width;
				}
				else if (texture->width < texture->height)
				{
					h = 64;
					w = 64 * texture->width / texture->height;
				}

				if (ImGui::ImageButton(ptr, ImVec2(f32(w), f32(h)), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), 2))
				{
					// TODO: select textures.
					// TODO: add hover functionality (tool tip - but show full texture + file name + size)
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();
		}
	}

	void update()
	{
		if (s_curLayer == 0)
		{
			static s32 __x = 0;
			__x++;
		}

		pushFont(TFE_Editor::FONT_SMALL);
		updateWindowControls();

		viewport_update((s32)UI_SCALE(480) + 16, (s32)UI_SCALE(68) + 18);
		viewport_render(s_view);

		// Toolbar
		toolbarBegin();
		{
			void* gpuPtr = TFE_RenderBackend::getGpuPtr(s_editCtrlToolbarData);
			if (drawToolbarButton(gpuPtr, 0, false))
			{
				// Play
			}
			ImGui::SameLine(0.0f, 32.0f);

			for (u32 i = 1; i < 6; i++)
			{
				if (drawToolbarButton(gpuPtr, i, i == s_editMode))
				{
					s_editMode = LevelEditMode(i);
				}
				ImGui::SameLine();
			}

			// Leave Boolean buttons off for now.

			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			if (ImGui::Combo("Grid Size", &s_gridIndex, c_gridSizes, IM_ARRAYSIZE(c_gridSizes)))
			{
				s_gridSize = c_gridSizeMap[s_gridIndex];
				s_gridSize2d = s_gridSize;
			}
			ImGui::PopItemWidth();
			// Get the "Grid Size" combo position to align the message panel later.
			const ImVec2 itemPos = ImGui::GetItemRectMin();

			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			s32 layerIndex = s_curLayer - s_level.layerRange[0];
			s32 minLayerIndex = s_level.layerRange[0] + 15;
			ImGui::Combo("Layer", &layerIndex, &s_layerStr[minLayerIndex], s_level.layerRange[1] - s_level.layerRange[0] + 1);
			s_curLayer = layerIndex + s_level.layerRange[0];
			ImGui::PopItemWidth();

			// Message Panel
			messagePanel(itemPos);
		}
		toolbarEnd();

		// Info Panel
		drawInfoPanel();

		// Browser
		drawBrowser();

		// Main viewport view.
		levelEditWinBegin();
		{
			const TextureGpu* texture = viewport_getTexture();
			ImGui::ImageButton(TFE_RenderBackend::getGpuPtr(texture), { (f32)s_viewportSize.x, (f32)s_viewportSize.z },
				{ 0.0f, 0.0f }, { 1.0f, 1.0f }, 0, { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
			const ImVec2 itemPos = ImGui::GetItemRectMin();
			s_editWinMapCorner = { itemPos.x, itemPos.y };

			// Display items on top of the viewport.
			s32 mx, my;
			TFE_Input::getMousePos(&mx, &my);
			const bool editWinHovered = mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z;

			if (s_view == EDIT_VIEW_2D && !getMenuActive())
			{
				// Display vertex info.
				if (s_hoveredVtxSector && s_hoveredVtxId >= 0 && editWinHovered)
				{
					// Give the "world space" vertex position, get back to the pixel position for the UI.
					const Vec2f vtx = s_hoveredVtxSector->vtx[s_hoveredVtxId];
					const Vec2i mapPos = worldPos2dToMap(vtx);

					const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
						| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;

					ImGui::SetNextWindowPos({ (f32)mapPos.x - UI_SCALE(20), (f32)mapPos.z - UI_SCALE(20) - 16 });
					ImGui::Begin("##VtxInfo", nullptr, window_flags);
					ImGui::Text("%d: %0.3f, %0.3f", s_hoveredVtxId, vtx.x, vtx.z);
					ImGui::End();
				}
			}
		}
		levelEditWinEnd();
		popFont();
	}

	////////////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////////////
	void resetZoom()
	{
		// We want to zoom into the mouse position.
		s32 relX = s_editWinSize.x / 2;
		s32 relY = s_editWinSize.z / 2;
		// Old position in world units.
		Vec2f worldPos;
		worldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
		worldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;

		s_zoom = c_defaultZoom;
		s_zoom2d = c_defaultZoom;

		// We want worldPos to stay put as we zoom
		Vec2f newWorldPos;
		newWorldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
		newWorldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;
		s_viewportPos.x += (worldPos.x - newWorldPos.x);
		s_viewportPos.z += (worldPos.z - newWorldPos.z);
	}

	void cameraControl2d(s32 mx, s32 my)
	{
		// WASD controls.
		const f32 moveSpd = s_zoom2d * f32(960.0 * TFE_System::getDeltaTime());
		if (TFE_Input::keyDown(KEY_W))
		{
			s_viewportPos.z -= moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_S))
		{
			s_viewportPos.z += moveSpd;
		}

		if (TFE_Input::keyDown(KEY_A))
		{
			s_viewportPos.x -= moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_D))
		{
			s_viewportPos.x += moveSpd;
		}

		// Mouse scrolling.
		if (TFE_Input::mouseDown(MBUTTON_RIGHT))
		{
			s32 dx, dy;
			TFE_Input::getMouseMove(&dx, &dy);

			s_viewportPos.x -= f32(dx) * s_zoom2d;
			s_viewportPos.z -= f32(dy) * s_zoom2d;
		}

		s32 dx, dy;
		TFE_Input::getMouseWheel(&dx, &dy);
		if (dy != 0)
		{
			// We want to zoom into the mouse position.
			s32 relX = s32(mx - s_editWinMapCorner.x);
			s32 relY = s32(my - s_editWinMapCorner.z);
			// Old position in world units.
			Vec2f worldPos;
			worldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
			worldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;

			s_zoom = std::max(s_zoom - f32(dy) * s_zoom * 0.1f, 1.0f/1024.0f);
			s_zoom = std::min(s_zoom, 4.0f);

			s_zoom2d = floorf(s_zoom * 1024.0f) / 1024.0f;

			// We want worldPos to stay put as we zoom
			Vec2f newWorldPos;
			newWorldPos.x = s_viewportPos.x + f32(relX) * s_zoom2d;
			newWorldPos.z = s_viewportPos.z + f32(relY) * s_zoom2d;
			s_viewportPos.x += (worldPos.x - newWorldPos.x);
			s_viewportPos.z += (worldPos.z - newWorldPos.z);
		}
	}
		
	void toolbarBegin()
	{
		bool toolbarActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("MainToolbar", { 0.0f, 22.0f });
		ImGui::SetWindowSize("MainToolbar", { (f32)displayInfo.width - 480.0f, 48.0f });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("MainToolbar", &toolbarActive, window_flags);
	}

	void toolbarEnd()
	{
		ImGui::End();
	}

	bool drawToolbarButton(void* ptr, u32 imageId, bool highlight)
	{
		const f32 imageHeightScale = 1.0f / 192.0f;
		const f32 y0 = f32(imageId) * 32.0f;
		const f32 y1 = y0 + 32.0f;

		ImGui::PushID(imageId);
		bool res = ImGui::ImageButton(ptr, ImVec2(32, 32), ImVec2(0.0f, y0 * imageHeightScale), ImVec2(1.0f, y1 * imageHeightScale), 0, ImVec4(0, 0, 0, highlight ? 0.75f : 0.25f), ImVec4(1, 1, 1, 1));
		ImGui::PopID();

		return res;
	}

	void levelEditWinBegin()
	{
		bool gridActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		s_editWinSize = { (s32)displayInfo.width - (s32)UI_SCALE(480), (s32)displayInfo.height - (s32)UI_SCALE(68) };

		ImGui::SetWindowPos("LevelEditWin", { (f32)s_editWinPos.x, (f32)s_editWinPos.z });
		ImGui::SetWindowSize("LevelEditWin", { (f32)s_editWinSize.x, (f32)s_editWinSize.z });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("LevelEditWin", &gridActive, window_flags);
	}

	void levelEditWinEnd()
	{
		ImGui::End();
	}

	Vec2f mouseCoordToWorldPos2d(s32 mx, s32 my)
	{
		// We want to zoom into the mouse position.
		s32 relX = s32(mx - s_editWinMapCorner.x);
		s32 relY = s32(my - s_editWinMapCorner.z);
		// Old position in world units.
		Vec2f worldPos;
		worldPos.x =   s_viewportPos.x + f32(relX) * s_zoom2d;
		worldPos.z = -(s_viewportPos.z + f32(relY) * s_zoom2d);
		return worldPos;
	}

	Vec2i worldPos2dToMap(const Vec2f& worldPos)
	{
		Vec2i mapPos;
		mapPos.x = s32(( worldPos.x - s_viewportPos.x) / s_zoom2d) + s_editWinMapCorner.x;
		mapPos.z = s32((-worldPos.z - s_viewportPos.z) / s_zoom2d) + s_editWinMapCorner.z;
		return mapPos;
	}

	void messagePanel(ImVec2 pos)
	{
		bool msgPanel = true;
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;

		ImGui::SetNextWindowPos({ pos.x, pos.y + 24.0f });
		ImGui::BeginChild("MsgPanel", { 280.0f, 20.0f }, false, window_flags);

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z && !getMenuActive())
		{
			if (s_view == EDIT_VIEW_2D)
			{
				Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
				// Assume the default zoom = 0.25 = 100%
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Zoom %0.2f%% : Pos %0.2f, %0.2f", 0.25f * 100.0f / s_zoom2d, worldPos.x, worldPos.z);
			}
			else
			{
				//ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Dir %0.3f, %0.3f, %0.3f   Sub-grid %0.2f", s_rayDir.x, s_rayDir.y, s_rayDir.z, s_gridSize / s_subGridSize);
			}
		}

		ImGui::EndChild();
	}
}
