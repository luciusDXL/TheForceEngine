#include "levelEditor.h"
#include "levelEditorData.h"
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
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_System/parser.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	// The TFE Level Editor format is different than the base format and contains extra 
	// metadata, etc.
	// So existing levels need to be loaded into that format.
	// If the correct format already exists, though, then it is loaded directly.
	EditorLevel s_level = {};
	u32 s_editFlags = LEF_DEFAULT;
	s32 s_curLayer = 0;

	EditorSector* s_hoveredSector = nullptr;
	EditorSector* s_selectedSector = nullptr;

	// Vertex
	EditorSector* s_hoveredVtxSector = nullptr;
	EditorSector* s_lastHoveredVtxSector = nullptr;
	EditorSector* s_selectedVtxSector = nullptr;
	s32 s_hoveredVtxId = -1;
	s32 s_selectedVtxId = -1;

	static EditorView s_view = EDIT_VIEW_2D;
	static LevelEditMode s_editMode = LEDIT_DRAW;
	static Vec2i s_editWinPos = { 0, 69 };
	static Vec2i s_editWinSize = { 0 };
	static Vec2f s_editWinMapCorner = { 0 };
	static f32 s_gridSize = 32.0f;
	static f32 s_zoom = 0.25f;

	static char s_layerMem[4 * 31];
	static char* s_layerStr[31];
		
	static s32 s_gridIndex = 6;
	static f32 c_gridSizeMap[] =
	{
		0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f
	};
	static const char* c_gridSizes[] =
	{
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
		"128",
		"256",
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

		s_hoveredSector  = nullptr;
		s_selectedSector = nullptr;

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
		
	void updateWindowControls()
	{
		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (!TFE_Input::relativeModeEnabled() && (mx < s_editWinPos.x || mx >= s_editWinPos.x + s_editWinSize.x || my < s_editWinPos.z || my >= s_editWinPos.z + s_editWinSize.z))
		{
			// Nothing is "hovered" if the mouse is not in the window.
			s_hoveredSector = nullptr;
			s_hoveredVtxSector = nullptr;
			s_lastHoveredVtxSector = nullptr;
			s_hoveredVtxId = -1;
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

				// DEBUG
				if (s_selectedSector && TFE_Input::keyPressed(KEY_T))
				{
					TFE_Polygon::computeTriangulation(&s_selectedSector->poly);
				}
			}
		}
	}

	void update()
	{
		pushFont(TFE_Editor::FONT_SMALL);
		updateWindowControls();

		viewport_update((s32)UI_SCALE(480) + 16, (s32)UI_SCALE(68) + 18);
		viewport_render(s_view);

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

			if (s_view == EDIT_VIEW_2D)
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
		ImGui::BeginChild("MsgPanel", { 256.0f, 20.0f }, false, window_flags);

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z)
		{
			if (s_view == EDIT_VIEW_2D)
			{
				Vec2f worldPos = mouseCoordToWorldPos2d(mx, my);
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Zoom %0.3f : Pos %0.2f, %0.2f", s_zoom2d, worldPos.x, worldPos.z);
			}
			else
			{
				//ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Dir %0.3f, %0.3f, %0.3f   Sub-grid %0.2f", s_rayDir.x, s_rayDir.y, s_rayDir.z, s_gridSize / s_subGridSize);
			}
		}

		ImGui::EndChild();
	}
}
