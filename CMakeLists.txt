cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

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

if (UNIX AND NOT APPLE)
	set(LINUX ON)
elseif (UNIX AND APPLE)
	set(MACOS ON)
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


add_executable(tfe)
set_target_properties(tfe PROPERTIES OUTPUT_NAME "theforceengine")

if(LINUX)
	find_package(PkgConfig REQUIRED)
	find_package(Threads REQUIRED)
	pkg_check_modules(RTAUDIO REQUIRED rtaudio>=5.2.0)
	pkg_check_modules(RTMIDI REQUIRED rtmidi>=5.0.0)
	pkg_check_modules(SDL2 REQUIRED sdl2)
	pkg_check_modules(GLEW REQUIRED glew)
	pkg_check_modules(IL REQUIRED IL)
	pkg_check_modules(ILU REQUIRED ILU)
	set(OpenGL_GL_PREFERENCE GLVND)
	find_package(OpenGL REQUIRED)
	target_include_directories(tfe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
	target_include_directories(tfe PRIVATE ${SDL2_INCLUDE_DIRS})
	target_link_libraries(tfe PRIVATE
				${OPENGL_LIBRARIES}
				${GLEW_LIBRARIES}
				${SDL2_LIBRARIES}
				${IL_LIBRARIES}
				${ILU_LIBRARIES}
				${RTAUDIO_LIBRARIES}
				${RTMIDI_LIBRARIES}
	)
	
	# set up build directory to be able to run TFE immediately: symlink
	# the necessary support file directories into the build env.
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/Captions)
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/Documentation)
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/Fonts)
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/Mods)
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/Shaders)
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/SoundFonts)
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/UI_Images)
	execute_process(COMMAND ln -sf ${CMAKE_SOURCE_DIR}/TheForceEngine/UI_Text)
	include(CreateGitVersionH.cmake)
	create_git_version_h()
endif()

target_include_directories(tfe PRIVATE TheForceEngine)

add_subdirectory(TheForceEngine/)


### installation ###

# Main binary
install(TARGETS tfe
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	BUNDLE  DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# Support data
install(DIRECTORY
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/Captions"
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/Documentation"
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/UI_Text"
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/UI_Images"
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/Shaders"
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/SoundFonts"
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/Fonts"
	"${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/Mods"
	DESTINATION "${CMAKE_INSTALL_DATADIR}"
        FILE_PERMISSIONS
          OWNER_READ OWNER_WRITE
          GROUP_READ
          WORLD_READ
        DIRECTORY_PERMISSIONS
          OWNER_READ OWNER_EXECUTE OWNER_WRITE
          GROUP_READ GROUP_EXECUTE GROUP_WRITE
          WORLD_READ WORLD_EXECUTE
)

# Linux .desktop files
if(LINUX)
	install(
		FILES "${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/TheForceEngine.desktop" DESTINATION "share/applications"
	)
	install(
		FILES "${CMAKE_CURRENT_SOURCE_DIR}/TheForceEngine/TheForceEngine.png" DESTINATION "${TFE_ICONDIR}/256x256/apps"
	)
endif()