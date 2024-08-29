cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
include(CheckCXXCompilerFlag)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# force Release by default.
if(NOT DEFINED CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

project(TheForceEngine
	HOMEPAGE_URL "https://theforceengine.github.io"
	DESCRIPTION "Modern 'Jedi Engine' replacement supporting Dark Forces, mods, and in the future Outlaws."
	)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# disable Clangs excessive warnings about unhandled switch values
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch")	
endif()

if (UNIX AND NOT APPLE)
	set(LINUX ON)
endif()

if(WIN32)
	# windows: drop everything into one folder
        set(CMAKE_INSTALL_BINDIR ".")
        set(CMAKE_INSTALL_LIBDIR ".")
        set(CMAKE_INSTALL_DATADIR ".")
else()
	set(TFE_ICONDIR "share/icons/hicolor")
	# tweak DATADIR to end up with ./share/TheForceEngine/
        set(CMAKE_INSTALL_DATADIR "share/${PROJECT_NAME}"
                CACHE PATH "Read-only architecture-independent data"
        )
endif()
include(GNUInstallDirs)
include(CreateGitVersionH.cmake)

##
## Options
##
option(ENABLE_TFE "Enable building “The Force Engine”" ON)
option(ENABLE_SYSMIDI "Enable System-MIDI Output if RTMidi is available" ON)
option(ENABLE_EDITOR "Enable TFE Editor" OFF)
option(ENABLE_FORCE_SCRIPT "Enable Force Script" OFF)
option(ENABLE_ADJUSTABLEHUD_MOD "Install the build‑in “AdjustableHud mod” with TFE" ON)


##
## Setup physfs for static linking
##
function(setup_physfs)
	option(PHYSFS_ARCHIVE_LECARCHIVES "" TRUE)
	option(PHYSFS_ARCHIVE_GRP "" FALSE)
	option(PHYSFS_ARCHIVE_WAD "" FALSE)
	option(PHYSFS_ARCHIVE_CSM "" FALSE)
	option(PHYSFS_ARCHIVE_HOG "" FALSE)
	option(PHYSFS_ARCHIVE_MVL "" FALSE)
	option(PHYSFS_ARCHIVE_QPAK "" FALSE)
	option(PHYSFS_ARCHIVE_SLB "" FALSE)
	option(PHYSFS_ARCHIVE_VDF "" FALSE)
	option(PHYSFS_ARCHIVE_ISO9660 "" FALSE)
	option(PHYSFS_BUILD_STATIC "" TRUE)
	option(PHYSFS_BUILD_SHARED "" FALSE)
	option(PHYSFS_BUILD_TEST "" FALSE)
	option(PHYSFS_DISABLE_INSTALL "" TRUE)
	add_subdirectory(extras/physfs)
	target_include_directories(tfe PRIVATE physfs-static)
	target_link_libraries(tfe PRIVATE PhysFS::PhysFS-static)
endfunction()



##
## Build
##
if(ENABLE_TFE)
	add_executable(tfe)
	set_target_properties(tfe PROPERTIES OUTPUT_NAME "theforceengine.bin")

	find_package(PkgConfig REQUIRED)
	find_package(SDL2 2.0.20 REQUIRED)
	pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
	target_include_directories(tfe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
	target_include_directories(tfe PRIVATE ${SDL2_INCLUDE_DIRS})
	target_include_directories(tfe PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
	setup_physfs()
	target_link_libraries(tfe PRIVATE SDL2::SDL2main SDL2::SDL2
				${SDL2_IMAGE_LIBRARIES}
	)


	if(ENABLE_SYSMIDI)
		pkg_check_modules(RTMIDI rtmidi>=5.0.0)
		if(RTMIDI_FOUND)
			add_definitions("-DBUILD_SYSMIDI")
			target_link_libraries(tfe PRIVATE ${RTMIDI_LIBRARIES})
		else()
			set(ENABLE_SYSMIDI 0)
			message(STATUS "System MIDI Disabled")
		endif()
	endif()
	if(ENABLE_EDITOR)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_EDITOR")
		set(ENABLE_FORCE_SCRIPT ON)
	endif()
	if(ENABLE_FORCE_SCRIPT)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_FORCE_SCRIPT")
	endif()


	if(ENABLE_FORCE_SCRIPT)
		target_include_directories(tfe PRIVATE "TheForceEngine/TFE_ForceScript/Angelscript/angelscript/include")
		target_include_directories(tfe PRIVATE "TheForceEngine/TFE_ForceScript/Angelscript/add_on")
	endif()

	create_git_version_h()

	target_include_directories(tfe PRIVATE TheForceEngine)
	add_subdirectory(TheForceEngine/)

	if(UNIX)
		# set up build directory to be able to run TFE immediately: symlink
		# the necessary support file directories into the build env.
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/Captions Captions SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/Documentation Documentation SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/Fonts Fonts SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/Mods Mods SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/Shaders Shaders SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/SoundFonts SoundFonts SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/UI_Images UI_Images SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/UI_Text UI_Text SYMBOLIC)
		file(CREATE_LINK ${CMAKE_SOURCE_DIR}/TheForceEngine/EditorDef EditorDef SYMBOLIC)
#		message(STATUS ${CMAKE_BINARY_DIR})
#		file(GLOB_RECURSE DATAFILES FOLLOW_SYMLINKS LIST_DIRECTORIES
#		TRUE "Captions Documentation Fonts Mods Shaders SoundFonts UI_Images UI_Text EditorDef")
#		message(STATUS "${DATAFILES}")
#		file(ARCHIVE_CREATE OUTPUT "theforceengine.data" FORMAT zip PATHS "${DATAFILES}")
	endif()

	add_custom_command(TARGET tfe POST_BUILD COMMAND zip ARGS -r9 ${CMAKE_BINARY_DIR}/theforceengine.data Captions/* Fonts/* Mods/* Shaders/* SoundFonts/* UI_Images/* UI_Text/* Documentation/*)
	add_custom_command(TARGET tfe POST_BUILD COMMAND cat ARGS theforceengine.bin theforceengine.data > theforceengine COMMAND chmod +x theforceengine)

endif()

### installation ###

if(ENABLE_TFE)
	# Main binary
	install(TARGETS tfe
		RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
		BUNDLE  DESTINATION "${CMAKE_INSTALL_BINDIR}"
	)

	# Linux .desktop files
	if(LINUX)
		install(
			FILES "${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/io.github.theforceengine.tfe.desktop" DESTINATION "share/applications"
		)
		install(
			FILES "${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/io.github.theforceengine.tfe.png" DESTINATION "${TFE_ICONDIR}/256x256/apps"
		)
		install(
			FILES "${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/io.github.theforceengine.tfe.metainfo.xml"
			DESTINATION "share/metainfo"
		)
	endif()
endif()
