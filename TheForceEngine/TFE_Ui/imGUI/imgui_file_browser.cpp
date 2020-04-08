#include "imgui_file_browser.h"
#include "imgui_internal.h"

#include <iostream>
#include <string.h>
#include <sstream>
#include <cwchar>
#include <cctype>
#include <algorithm>
#include <cmath>

#if defined (WIN32) || defined (_WIN32) || defined (__WIN32)
#define OSWIN
#include "Dirent/dirent.h"
#include <windows.h>
#undef min
#undef max
#else
#include <dirent.h>
#endif // defined (WIN32) || defined (_WIN32)

namespace imgui_addons
{
	ImGuiFileBrowser::ImGuiFileBrowser()
	{
		is_dir = false;
		show_hidden = false;
		filter_dirty = true;
		col_items_limit = 12;
		selected_idx = -1;
		selected_ext_idx = 0;
		col_width = 280.0f;
		setNonDefaultRoot = false;
#ifdef OSWIN
		current_path = "./";
#else
		current_path = "/";
		current_dirlist.push_back("/");
#endif
	}

	ImGuiFileBrowser::~ImGuiFileBrowser()
	{
			   
	}

	void ImGuiFileBrowser::setCurrentPath(std::string path)
	{
		current_path = path;
		// replace \\ with /
		size_t len = path.length();
		for (size_t i = 0; i < len; i++)
		{
			if (current_path[i] == '\\')
			{
				current_path[i] = '/';
			}
		}
		setNonDefaultRoot = true;
	}

	void ImGuiFileBrowser::closeDialog()
	{
		selected_ext_idx = 0;
		selected_idx = -1;
		save_fn[0] = '\0';  //Hide any text in Input bar for the next time save dialog is opened.
		filter.Clear();     //Clear Filter for the next time open dialog is called.
		filter_dirty = true;
		ImGui::CloseCurrentPopup();
	}

	bool ImGuiFileBrowser::showOpenFileDialog(std::string label, ImVec2 sz_xy, std::string valid_types)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowContentSize(sz_xy);
		if (ImGui::BeginPopupModal(label.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			/* Initialize a list of files extensions that are valid. If the user chooses a file that doesn't match
			 * the extensions in the list, show an error modal...
			 */
			if (this->valid_types != valid_types)
			{
				this->valid_types = valid_types;
				valid_exts.clear();
				std::string ext = "";
				std::istringstream iss(valid_types);
				while (std::getline(iss, ext, ','))
				{
					if (!ext.empty())
						valid_exts.push_back(ext);
				}
			}
			selected_fn.clear();
			bool show_error = false;
			int filtered_items = 0;

			/* If subdirs and subfiles are empty, either we are on Unix OS or loadWindowsDrives() failed.
			 * Hence read default directory (./) on Windows and "/" on Unix OS once
			 */
			if (subdirs.empty() && subfiles.empty())
				show_error |= !(readDIR(current_path));

			float frame_height = ImGui::GetFrameHeight();
			float frame_height_spacing = ImGui::GetFrameHeightWithSpacing();

			//Render top file bar for easy navigation
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.882f, 0.745f, 0.078f, 1.0f));
			for (int i = 0; i < current_dirlist.size(); i++)
			{
				if (ImGui::Button(current_dirlist[i].c_str()))
				{
					//If last button clicked, nothing happens
					if (i == current_dirlist.size() - 1)
						break;
					show_error |= !(onNavigationButtonClick(i));
				}

				//Draw Arrow Buttons
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.01f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				if (i != current_dirlist.size() - 1)
				{
					ImGui::SameLine(0, 0);
					ImGui::ArrowButtonEx("##Right", ImGuiDir_Right, ImVec2(frame_height, frame_height), ImGuiButtonFlags_Disabled);
					ImGui::SameLine(0, 0);
				}
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
			}
			ImGui::PopStyleColor();

			ImGui::Separator();
			ImVec2 cursor_pos = ImGui::GetCursorPos();
			ImGui::SetCursorPosY(sz_xy.y - frame_height_spacing * 1.5f);

			//Filter items if filter text changed or the contents change due to reading new directory, hidden checkbox etc,
			if (filter.Draw("Filter (inc, -exc)", sz_xy.x - 145) || filter_dirty)
			{
				filter_dirty = false;
				filtered_dirs.clear();
				filtered_files.clear();
				for (int i = 0; i < subdirs.size(); i++)
				{
					if (filter.PassFilter(subdirs[i].name.c_str()))
						filtered_dirs.push_back(&subdirs[i]);
				}

				for (int i = 0; i < subfiles.size(); i++)
				{
					if (filter.PassFilter(subfiles[i].name.c_str()))
						filtered_files.push_back(&subfiles[i]);
				}
			}

			//If filter bar was focused clear selection
			if (ImGui::GetFocusID() == ImGui::GetID("Filter (inc, -exc)"))
				selected_idx = -1;

			//Reinitialize the limit on number of selectables in one column based on height
			col_items_limit = int((sz_xy.y - 119) / 17);
			int num_cols = (int)std::max(1.0f, std::ceil((filtered_dirs.size() + filtered_files.size()) / (float)col_items_limit));
			float content_width = std::max(sz_xy.x - ImGui::GetStyle().WindowPadding.x * 2, num_cols * col_width);

			ImGui::SetCursorPos(cursor_pos);
			ImGui::SetNextWindowContentSize(ImVec2(content_width, 0.0f));
			ImGui::BeginChild("##ScrollingRegion", ImVec2(0, -70), true, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::Columns(num_cols);

			//Output directories in yellow
			bool show_drives = false;
#ifdef OSWIN
			(current_dirlist.size() && current_dirlist.back() == "Computer") ? show_drives = true : show_drives = false;
#endif // OSWIN

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.882f, 0.745f, 0.078f, 1.0f));
			for (int i = 0; i < filtered_dirs.size(); i++)
			{
				if (!filtered_dirs[i]->is_hidden || show_hidden)
				{
					filtered_items++;
					if (ImGui::Selectable(filtered_dirs[i]->name.c_str(), selected_idx == i && is_dir, ImGuiSelectableFlags_AllowDoubleClick))
					{
						selected_idx = i;
						is_dir = true;
						if (ImGui::IsMouseDoubleClicked(0))
							show_error |= !(onDirClick(i, show_drives, false));
					}
					if ((filtered_items) % col_items_limit == 0)
						ImGui::NextColumn();
				}
			}
			ImGui::PopStyleColor(1);

			//Output files
			for (int i = 0; i < filtered_files.size(); i++)
			{
				if (!filtered_files[i]->is_hidden || show_hidden)
				{
					filtered_items++;
					if (ImGui::Selectable(filtered_files[i]->name.c_str(), selected_idx == i && !is_dir, ImGuiSelectableFlags_AllowDoubleClick))
					{
						selected_idx = i;
						is_dir = false;
						if (ImGui::IsMouseDoubleClicked(0))
							selected_fn = current_path + filtered_files[i]->name;
					}
					if ((filtered_items) % col_items_limit == 0)
						ImGui::NextColumn();
				}
			}
			ImGui::Columns(1);
			ImGui::EndChild();

			//Draw Remaining UI elements
			ImGui::SetCursorPosY(ImGui::GetWindowSize().y - frame_height_spacing - ImGui::GetStyle().WindowPadding.y);
			ImGui::Checkbox("Show Hidden Files and Folders", &show_hidden);
			ImGui::SameLine();

			ImGui::SetCursorPosX(sz_xy.x - 100);
			if (ImGui::Button("Open", ImVec2(50, 0)))
			{
				if (selected_idx >= 0)
				{
					if (is_dir)
						show_error |= !(onDirClick(selected_idx, show_drives, false));
					else
						selected_fn = current_path + subfiles[selected_idx].name;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(50, 0)))
				closeDialog();

			//If a file was selected, check if the file extension is supported.
			if (!selected_fn.empty() && !valid_exts.empty())
			{
				if (!validateFile())
				{
					ImGui::OpenPopup("Invalid File!");
					selected_fn.clear();
				}
			}
			showInvalidFileModal();

			//Show Error Modal if there was an error opening any directory
			if (show_error)
				ImGui::OpenPopup(error_title.c_str());
			showErrorModal();

			//If selected file passes through validation check, close file dialog
			if (!selected_fn.empty())
				closeDialog();
			ImGui::EndPopup();
			return (!selected_fn.empty());
		}
		else
			return false;

	}

	bool ImGuiFileBrowser::showSaveFileDialog(std::string label, ImVec2 sz_xy, std::string save_types)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowContentSize(sz_xy);
		if (ImGui::BeginPopupModal(label.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (this->valid_types != save_types)
			{
				this->valid_types = save_types;
				valid_exts.clear();
				std::string ext = "";
				std::istringstream iss(save_types);
				while (std::getline(iss, ext, ','))
				{
					if (!ext.empty())
						valid_exts.push_back(ext);
				}
			}
			selected_fn.clear();
			bool show_error = false;

			/* If subdirs and subfiles are empty, either we are on Unix OS or loadWindowsDrives() failed.
			 * Hence read default directory (./) on Windows and "/" on Unix OS once
			 */
			if (subdirs.empty() && subfiles.empty())
				show_error |= !(readDIR(current_path));

			float frame_height = ImGui::GetFrameHeight();
			float frame_height_spacing = ImGui::GetFrameHeightWithSpacing();

			//Render top file bar for easy navigation
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.882f, 0.745f, 0.078f, 1.0f));
			for (int i = 0; i < current_dirlist.size(); i++)
			{
				if (ImGui::Button(current_dirlist[i].c_str()))
				{
					//If last button clicked, nothing happens
					if (i == current_dirlist.size() - 1)
						break;
					show_error |= !(onNavigationButtonClick(i));
				}

				//Draw Arrow Buttons
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.01f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				if (i != current_dirlist.size() - 1)
				{
					ImGui::SameLine(0, 0);
					ImGui::ArrowButtonEx("##Right", ImGuiDir_Right, ImVec2(frame_height, frame_height), ImGuiButtonFlags_Disabled);
					ImGui::SameLine(0, 0);
				}
				ImGui::PopStyleColor();
				ImGui::PopStyleColor();
			}
			ImGui::PopStyleColor();

			ImGui::Separator();

			//Reinitialize the limit on number of selectables in one column based on height
			col_items_limit = int((sz_xy.y - 119) / 17);
			int num_cols = (int)std::max(1.0f, std::ceil((subdirs.size() + subfiles.size()) / (float)col_items_limit));
			float content_width = std::max(sz_xy.x - ImGui::GetStyle().WindowPadding.x * 2, num_cols * col_width);

			ImGui::SetNextWindowContentSize(ImVec2(content_width, 0.0f));
			ImGui::BeginChild("##ScrollingRegion", ImVec2(0, -70), true, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::Columns(num_cols);

			//Output directories in yellow
			bool show_drives = false;
#ifdef OSWIN
			(current_dirlist.back() == "Computer") ? show_drives = true : show_drives = false;
#endif // OSWIN

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.882f, 0.745f, 0.078f, 1.0f));
			int items = 0;
			for (int i = 0; i < subdirs.size(); i++)
			{
				if (!subdirs[i].is_hidden || show_hidden)
				{
					items++;
					if (ImGui::Selectable(subdirs[i].name.c_str(), selected_idx == i && is_dir, ImGuiSelectableFlags_AllowDoubleClick))
					{
						selected_idx = i;
						is_dir = true;
						if (ImGui::IsMouseDoubleClicked(0))
							show_error |= !(onDirClick(i, show_drives, true));
					}
					if ((items) % col_items_limit == 0)
						ImGui::NextColumn();
				}
			}
			ImGui::PopStyleColor(1);

			//Output files
			for (int i = 0; i < subfiles.size(); i++)
			{
				if (!subfiles[i].is_hidden || show_hidden)
				{
					items++;
					if (ImGui::Selectable(subfiles[i].name.c_str(), selected_idx == i && !is_dir))
					{
						int len = (int)subfiles[i].name.length();
						selected_idx = i;
						is_dir = false;
						if (len < 500)
						{
							subfiles[i].name.copy(save_fn, len, 0);
							save_fn[len] = '\0';
						}
					}
					if ((items) % col_items_limit == 0)
						ImGui::NextColumn();
				}
			}
			ImGui::Columns(1);
			ImGui::EndChild();

			//Draw Remaining UI elements
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

			//Show a Text Input for typing save file name
			ImGui::Text("Save As: ");
			ImGui::SameLine();

			ImGui::PushItemWidth(sz_xy.x - 160);
			if (ImGui::InputTextWithHint("##SaveFileNameInput", "Type a filename with extension", &save_fn[0], 500, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (strlen(save_fn) > 0)
					selected_fn = current_path + std::string(save_fn);
			}
			ImGui::PopItemWidth();
			ImGui::SameLine();
			ImGui::PushItemWidth(80);
			if (ImGui::BeginCombo("##saveTypes", valid_exts[selected_ext_idx].c_str()))
			{
				for (int i = 0; i < valid_exts.size(); i++)
				{
					if (ImGui::Selectable(valid_exts[i].c_str(), selected_ext_idx == i))
					{
						selected_ext_idx = i;
						std::string name(save_fn);
						size_t idx = name.find_last_of(".");
						if (idx == std::string::npos)
							idx = strlen(save_fn);
						for (int i = 0; i < valid_exts[selected_ext_idx].size(); i++)
							save_fn[idx++] = valid_exts[selected_ext_idx][i];
						save_fn[idx++] = '\0';
					}
				}
				ImGui::EndCombo();
			}
			ext = valid_exts[selected_ext_idx];
			ImGui::PopItemWidth();

			ImGui::SetCursorPosY(ImGui::GetWindowSize().y - frame_height_spacing - ImGui::GetStyle().WindowPadding.y);
			ImGui::Checkbox("Show Hidden Files and Folders", &show_hidden);
			ImGui::SameLine();

			ImGui::SetCursorPosX(sz_xy.x - 100);
			if (selected_idx != -1 && is_dir && ImGui::GetFocusID() != ImGui::GetID("##SaveFileNameInput"))
			{
				if (ImGui::Button("Open", ImVec2(50, 0)))
					show_error |= !(onDirClick(selected_idx, show_drives, true));
			}
			else
			{
				//If input bar was focused clear selection
				if (ImGui::GetFocusID() == ImGui::GetID("##SaveFileNameInput"))
					selected_idx = -1;

				if (ImGui::Button("Save", ImVec2(50, 0)))
				{
					if (strlen(save_fn) > 0)
						selected_fn = current_path + std::string(save_fn);
					else
					{
						show_error = true;
						error_title = "Invalid Filename!";
						error_msg = "Please type a file name.";
					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(50, 0)))
				closeDialog();

			//Show Error Modal if there was an error opening any directory
			if (show_error)
				ImGui::OpenPopup(error_title.c_str());
			showErrorModal();

			//If selected file passes through validation check, close file dialog
			if (!selected_fn.empty())
				closeDialog();
			ImGui::EndPopup();
			return (!selected_fn.empty());
		}
		else
			return false;
	}

	bool ImGuiFileBrowser::onNavigationButtonClick(int idx)
	{
		std::string new_path(current_path);

		//First Button corresponds to virtual folder Computer which lists all logical drives (hard disks and removables) and "/" on Unix
		if (idx == 0)
		{
#ifdef OSWIN
			if (!loadWindowsDrives())
				return false;
			current_path.clear();
			current_dirlist.clear();
			current_dirlist.push_back("Computer");
			return true;
#else
			new_path = "/";
#endif // OSWIN
		}
		else
		{
#ifdef OSWIN
			//Clicked on a drive letter?
			if (idx == 1)
				new_path = current_path.substr(0, 3);
			else
				new_path = current_path.substr(0, current_path.find("/" + current_dirlist[idx]) + current_dirlist[idx].length() + 2);
#else
			//find returns 0 based indices and substr takes length of chars thus + 1. Another +1 to include trailing "/"
			new_path = current_path.substr(0, current_path.find("/" + current_dirlist[idx]) + current_dirlist[idx].length() + 2);
#endif
		}

		if (readDIR(new_path))
		{
			current_dirlist.erase(current_dirlist.begin() + idx + 1, current_dirlist.end());
			current_path = new_path;
			return true;
		}
		else
			return false;
	}

	bool ImGuiFileBrowser::onDirClick(int idx, bool show_drives, bool is_save_dialog)
	{
		std::string name;
		std::string new_path(current_path);

		if (is_save_dialog)
			name = subdirs[idx].name;
		else
			name = filtered_dirs[idx]->name;

		if (name == "..")
		{
			new_path.pop_back(); //Remove trailing "/"
			new_path = new_path.substr(0, new_path.find_last_of("/") + 1); //Also include a trailing "/"
		}
		else
		{
#ifdef OSWIN
			if (show_drives)
			{
				//Remember we displayed drives as *Local/Removable Disk: X* hence we need last char only
				name = std::string(1, name.back()) + ":";
				new_path += name + "/";
			}
			else
				new_path += name + "/";
#else
			new_path += name + "/";
#endif // OSWIN
		}

		if (readDIR(new_path))
		{
			if (name == "..")
				current_dirlist.pop_back();
			else
				current_dirlist.push_back(name);

			current_path = new_path;
			return true;
		}
		else
			return false;
	}

	bool ImGuiFileBrowser::readDIR(std::string pathdir)
	{
		DIR* dir;
		struct dirent *ent;
		if ((dir = opendir(pathdir.c_str())) != NULL)
		{
#ifdef OSWIN
			// If we are on Windows and failed to load Windows Drives, populate filebar with default path
			if (current_dirlist.empty() && (pathdir == "./" || setNonDefaultRoot))
			{
				setNonDefaultRoot = false;
				const wchar_t* absolute_path = dir->wdirp->patt;
				std::string current_directory = wStringToString(absolute_path);
				std::replace(current_directory.begin(), current_directory.end(), '\\', '/');

				//Remove trailing "/*" returned by ** dir->wdirp->patt **
				current_directory.pop_back();
				current_path = current_directory;

				//Create a vector of each directory in the file path for the filepath bar. Not Necessary for linux as starting directory is "/"
				parsePathTabs(current_path);
			}
#endif // OSWIN

			// store all the files and directories within directory anc clear previous entries
			clearOldEntries();
			while ((ent = readdir(dir)) != NULL)
			{
				bool is_hidden = false;
				std::string name(ent->d_name);

				//Ignore current directory
				if (name == ".")
					continue;

				//Somehow there is a '..' present in root directory in linux.
#ifndef OSWIN
				if (name == ".." && pathdir == "/")
					continue;
#endif // OSWIN

				if (name != "..")
				{
#ifdef OSWIN
					std::string dir = pathdir + std::string(ent->d_name);
					// IF system file skip it...
					if (FILE_ATTRIBUTE_SYSTEM & GetFileAttributesA(dir.c_str()))
						continue;
					if (FILE_ATTRIBUTE_HIDDEN & GetFileAttributesA(dir.c_str()))
						is_hidden = true;
#else
					if (name[0] == '.')
						is_hidden = true;
#endif // OSWIN
				}
				//Store directories and files in separate vectors
				if (ent->d_type == DT_DIR)
					subdirs.push_back(Info(name, is_hidden));
				else if (ent->d_type == DT_REG)
					subfiles.push_back(Info(name, is_hidden));
			}
			closedir(dir);
			std::sort(subdirs.begin(), subdirs.end(), alphaSortComparator);
			std::sort(subfiles.begin(), subfiles.end(), alphaSortComparator);
		}
		else
		{
			error_title = "Error!";
			error_msg = "Error opening directory! Make sure you have the proper rights to access the directory.";
			return false;
		}
		return true;
	}

	void ImGuiFileBrowser::showErrorModal()
	{
		ImVec2 text_size = ImGui::CalcTextSize(error_msg.c_str(), NULL, false, 250);
		ImVec2 window_size(250, 0);
		ImGui::SetNextWindowSize(window_size);

		if (ImGui::BeginPopupModal(error_title.c_str(), NULL, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((window_size.x - text_size.x) / 2));
			ImGui::TextWrapped("%s", error_msg.c_str());

			ImGui::Separator();
			ImGui::SetCursorPosX(window_size.x / 2.0f - 25.0f);
			if (ImGui::Button("Ok", ImVec2(50, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}

	void ImGuiFileBrowser::showInvalidFileModal()
	{
		int height = (int)std::min(180.0, 125.0 + valid_exts.size() * 17);
		ImVec2 window_size((float)350, (float)height);
		ImGui::SetNextWindowSize(window_size);

		if (ImGui::BeginPopupModal("Invalid File!", NULL, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
		{
			float frame_height = ImGui::GetFrameHeightWithSpacing();

			std::string text = "Selected file is not supported. Please select a file with the following extensions...";
			ImGui::TextWrapped("%s", text.c_str());

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
			ImGui::SetNextWindowContentSize(ImVec2(0.0f, float(17 * valid_exts.size())));
			ImGui::BeginChild("##SupportedExts", ImVec2(0, -35), true);
			for (int i = 0; i < valid_exts.size(); i++)
				ImGui::BulletText("%s", valid_exts[i].c_str());
			ImGui::EndChild();

			ImGui::SetCursorPos(ImVec2(window_size.x / 2.0f - 25.0f, height - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y));
			if (ImGui::Button("Ok", ImVec2(50, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}

	bool ImGuiFileBrowser::validateFile()
	{
		int idx = (int)selected_fn.find_last_of(".");
		std::string ext = selected_fn.substr(idx, selected_fn.length() - idx);
		return (std::find(valid_exts.begin(), valid_exts.end(), ext) != valid_exts.end());
	}

	void ImGuiFileBrowser::parsePathTabs(std::string path)
	{
		std::string path_element = "";
		std::string root = "";

		current_dirlist.push_back("Computer");

		std::istringstream iss(path);
		while (std::getline(iss, path_element, '/'))
		{
			if (!path_element.empty())
				current_dirlist.push_back(path_element);
		}
	}

	std::string ImGuiFileBrowser::wStringToString(const wchar_t* wchar_arr)
	{
		std::mbstate_t state = std::mbstate_t();

		//MinGW bug (patched in mingw-w64), wcsrtombs doesn't ignore length parameter when dest = null. Hence the large number.
		size_t len = 1 + std::wcsrtombs(NULL, &(wchar_arr), 600000, &state);

		char char_arr[4096];	// This is a bug - they didn't test with visual studio.
		std::wcsrtombs(char_arr, &wchar_arr, len, &state);
		return std::string(char_arr);
	}

	bool ImGuiFileBrowser::alphaSortComparator(const Info& a, const Info& b)
	{
		const char* str1 = a.name.c_str();
		const char* str2 = b.name.c_str();
		int ca, cb;
		do
		{
			ca = (unsigned char)*str1++;
			cb = (unsigned char)*str2++;
			ca = std::tolower(std::toupper(ca));
			cb = std::tolower(std::toupper(cb));
		} while (ca == cb && ca != '\0');
		if (ca - cb <= 0)
			return true;
		else
			return false;
	}

	void ImGuiFileBrowser::clearOldEntries()
	{
		//Clear pointer references to subdirs and subfiles
		filtered_dirs.clear();
		filtered_files.clear();

		//Now clear subdirs and subfiles
		subdirs.clear();
		subfiles.clear();
		filter_dirty = true;
		selected_idx = -1;
	}

	//Windows Exclusive function
#ifdef OSWIN
	bool ImGuiFileBrowser::loadWindowsDrives()
	{
		DWORD len = GetLogicalDriveStringsA(0, NULL);
		char drives[4096];	// Another case not tested with visual studio
		if (!GetLogicalDriveStringsA(len, drives))
			return false;

		clearOldEntries();
		char* temp = drives;
		for (char *drv = NULL; *temp != NULL; temp++)
		{
			drv = temp;
			if (DRIVE_REMOVABLE == GetDriveTypeA(drv))
				subdirs.push_back({ "Removable Disk: " + std::string(1,drv[0]), false });
			else if (DRIVE_FIXED == GetDriveTypeA(drv))
				subdirs.push_back({ "Local Disk: " + std::string(1,drv[0]), false });
			//Go to null character
			while (*(++temp));
		}
		return true;
	}
#endif
}