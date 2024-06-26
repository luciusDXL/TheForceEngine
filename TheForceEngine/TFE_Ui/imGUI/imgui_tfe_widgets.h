#ifndef _IMGUI_TFE_WIDGETS_H_
#define _IMGUI_TFE_WIDGETS_H_

namespace ImGui {
IMGUI_API bool ImageAnimButton(ImTextureID texture_id, ImTextureID texture_id2, const ImVec2& image_size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec2& uv2 = ImVec2(0, 0), const ImVec2& uv3 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1));
IMGUI_API bool ImageButtonDualTint(ImTextureID user_texture_id, const ImVec2& image_size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& bg_col = ImVec4(0, 0, 0, 0), const ImVec4& tint_col = ImVec4(1, 1, 1, 1), const ImVec4& tint_col2 = ImVec4(1, 1, 1, 1));
IMGUI_API bool InputUInt(const char* label, unsigned int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);
}

#endif // _IMGUI_TFE_WIDGETS_H_
