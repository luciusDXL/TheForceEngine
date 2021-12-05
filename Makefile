#
# WIP Makefile for GNU/Linux
#
# Subject to change, this will eventually be replaced by a proper multi-plat
# build system such as CMake
#
# Adjust AUDIODEF and AUDIOLIB against RtAudio accordingly
#

CC			=	cc
CXX			=	c++
AUDIODEF	=	-I/usr/include/rtaudio -I/usr/include/rtmidi -D__LINUX_ALSA__
AUDIOLIB	=	-lasound -lrtaudio -lrtmidi
CFLAGS		=	-g -I/usr/include/SDL2 -I./TheForceEngine $(AUDIODEF)
CXXFLAGS	=	$(CFLAGS)
LDFLAGS		=	-g -pthread -lGL -lGLEW -lSDL2 -lIL -lILU $(AUDIOLIB)

BIN			=	TheForceEngine/tfe

ARCHIVE		=	TheForceEngine/TFE_Archive/archive.o \
				TheForceEngine/TFE_Archive/gobArchive.o \
				TheForceEngine/TFE_Archive/gobMemoryArchive.o \
				TheForceEngine/TFE_Archive/labArchive.o \
				TheForceEngine/TFE_Archive/lfdArchive.o \
				TheForceEngine/TFE_Archive/zipArchive.o \
				TheForceEngine/TFE_Archive/zip/zip.o

ASSET		=	TheForceEngine/TFE_Asset/assetSystem.o \
				TheForceEngine/TFE_Asset/colormapAsset.o \
				TheForceEngine/TFE_Asset/dfKeywords.o \
				TheForceEngine/TFE_Asset/fontAsset.o \
				TheForceEngine/TFE_Asset/gameMessages.o \
				TheForceEngine/TFE_Asset/gifWriter.o \
				TheForceEngine/TFE_Asset/gmidAsset.o \
				TheForceEngine/TFE_Asset/imageAsset.o \
				TheForceEngine/TFE_Asset/levelList.o \
				TheForceEngine/TFE_Asset/modelAsset_jedi.o \
				TheForceEngine/TFE_Asset/paletteAsset.o \
				TheForceEngine/TFE_Asset/spriteAsset_Jedi.o \
				TheForceEngine/TFE_Asset/textureAsset.o \
				TheForceEngine/TFE_Asset/vocAsset.o \
				TheForceEngine/TFE_Asset/vueAsset.o

AUDIO		=	TheForceEngine/TFE_Audio/audioDevice.o \
				TheForceEngine/TFE_Audio/audioSystem.o \
				TheForceEngine/TFE_Audio/midiDevice.o \
				TheForceEngine/TFE_Audio/midiPlayer.o \

DARKFORCES	=	TheForceEngine/TFE_DarkForces/Actor/actor.o \
				TheForceEngine/TFE_DarkForces/Actor/actorDebug.o \
				TheForceEngine/TFE_DarkForces/Actor/bobaFett.o \
				TheForceEngine/TFE_DarkForces/Actor/dragon.o \
				TheForceEngine/TFE_DarkForces/Actor/enemies.o \
				TheForceEngine/TFE_DarkForces/Actor/exploders.o \
				TheForceEngine/TFE_DarkForces/Actor/flyers.o \
				TheForceEngine/TFE_DarkForces/Actor/mousebot.o \
				TheForceEngine/TFE_DarkForces/Actor/phaseOne.o \
				TheForceEngine/TFE_DarkForces/Actor/phaseThree.o \
				TheForceEngine/TFE_DarkForces/Actor/phaseTwo.o \
				TheForceEngine/TFE_DarkForces/Actor/scenery.o \
				TheForceEngine/TFE_DarkForces/Actor/sewer.o \
				TheForceEngine/TFE_DarkForces/Actor/troopers.o \
				TheForceEngine/TFE_DarkForces/Actor/turret.o \
				TheForceEngine/TFE_DarkForces/Actor/welder.o \
				TheForceEngine/TFE_DarkForces/GameUI/agentMenu.o \
				TheForceEngine/TFE_DarkForces/GameUI/delt.o \
				TheForceEngine/TFE_DarkForces/GameUI/editBox.o \
				TheForceEngine/TFE_DarkForces/GameUI/escapeMenu.o \
				TheForceEngine/TFE_DarkForces/GameUI/uiDraw.o \
				TheForceEngine/TFE_DarkForces/agent.o \
				TheForceEngine/TFE_DarkForces/animLogic.o \
				TheForceEngine/TFE_DarkForces/automap.o \
				TheForceEngine/TFE_DarkForces/cheats.o \
				TheForceEngine/TFE_DarkForces/config.o \
				TheForceEngine/TFE_DarkForces/darkForcesMain.o \
				TheForceEngine/TFE_DarkForces/gameList.o \
				TheForceEngine/TFE_DarkForces/gameMessage.o \
				TheForceEngine/TFE_DarkForces/generator.o \
				TheForceEngine/TFE_DarkForces/hitEffect.o \
				TheForceEngine/TFE_DarkForces/hud.o \
				TheForceEngine/TFE_DarkForces/item.o \
				TheForceEngine/TFE_DarkForces/logic.o \
				TheForceEngine/TFE_DarkForces/mission.o \
				TheForceEngine/TFE_DarkForces/pickup.o \
				TheForceEngine/TFE_DarkForces/player.o \
				TheForceEngine/TFE_DarkForces/playerCollision.o \
				TheForceEngine/TFE_DarkForces/projectile.o \
				TheForceEngine/TFE_DarkForces/random.o \
				TheForceEngine/TFE_DarkForces/time.o \
				TheForceEngine/TFE_DarkForces/updateLogic.o \
				TheForceEngine/TFE_DarkForces/util.o \
				TheForceEngine/TFE_DarkForces/vueLogic.o \
				TheForceEngine/TFE_DarkForces/weapon.o \
				TheForceEngine/TFE_DarkForces/weaponFireFunc.o

FILESYSTEM	=	TheForceEngine/TFE_FileSystem/filestream.o \
				TheForceEngine/TFE_FileSystem/fileutil.o \
				TheForceEngine/TFE_FileSystem/paths.o

FRONTENDUI	=	TheForceEngine/TFE_FrontEndUI/console.o \
				TheForceEngine/TFE_FrontEndUI/editorTexture.o \
				TheForceEngine/TFE_FrontEndUI/frontEndUi.o \
				TheForceEngine/TFE_FrontEndUI/modLoader.o \
				TheForceEngine/TFE_FrontEndUI/profilerView.o

GAME		=	TheForceEngine/TFE_Game/igame.o

INPUT		=	TheForceEngine/TFE_Input/input.o \
				TheForceEngine/TFE_Input/inputMapping.o

JEDI		=	TheForceEngine/TFE_Jedi/Collision/collision.o \
				TheForceEngine/TFE_Jedi/InfSystem/infSystem.o \
				TheForceEngine/TFE_Jedi/InfSystem/message.o \
				TheForceEngine/TFE_Jedi/Level/level.o \
				TheForceEngine/TFE_Jedi/Level/rfont.o \
				TheForceEngine/TFE_Jedi/Level/robject.o \
				TheForceEngine/TFE_Jedi/Level/roffscreenBuffer.o \
				TheForceEngine/TFE_Jedi/Level/rsector.o \
				TheForceEngine/TFE_Jedi/Level/rtexture.o \
				TheForceEngine/TFE_Jedi/Level/rwall.o \
				TheForceEngine/TFE_Jedi/Math/core_math.o \
				TheForceEngine/TFE_Jedi/Math/cosTable.o \
				TheForceEngine/TFE_Jedi/Memory/allocator.o \
				TheForceEngine/TFE_Jedi/Memory/list.o \
				TheForceEngine/TFE_Jedi/Renderer/jediRenderer.o \
				TheForceEngine/TFE_Jedi/Renderer/rcommon.o \
				TheForceEngine/TFE_Jedi/Renderer/rscanline.o \
				TheForceEngine/TFE_Jedi/Renderer/rsectorRender.o \
				TheForceEngine/TFE_Jedi/Renderer/screenDraw.o \
				TheForceEngine/TFE_Jedi/Renderer/virtualFramebuffer.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixed.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixedSharedState.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/redgePairFixed.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rflatFixed.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rlightingFixed.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rsectorFixed.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rwallFixed.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_Clipping.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_Culling.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_PolygonDraw.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_PolygonSetup.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_TransformAndLighting.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rclassicFloat.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rclassicFloatSharedState.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/redgePairFloat.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rflatFloat.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rlightingFloat.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rsectorFloat.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rwallFloat.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_Clipping.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_Culling.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_PolygonDraw.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_PolygonSetup.o \
				TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_TransformAndLighting.o \
				TheForceEngine/TFE_Jedi/Sound/soundSystem.o \
				TheForceEngine/TFE_Jedi/Task/task.o

MEMORY		=	TheForceEngine/TFE_Memory/chunkedArray.o \
				TheForceEngine/TFE_Memory/memoryRegion.o

OUTLAWS		=	TheForceEngine/TFE_Outlaws/outlawsMain.o

POLYGON		=	TheForceEngine/TFE_Polygon/clipper.o \
				TheForceEngine/TFE_Polygon/polygon.o

POSTPROCESS	=	TheForceEngine/TFE_PostProcess/blit.o \
				TheForceEngine/TFE_PostProcess/postprocess.o

RENDER		=	TheForceEngine/TFE_RenderBackend/Win32OpenGL/dynamicTexture.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/glslParser.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/indexBuffer.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/openGL_Caps.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/renderBackend.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/renderState.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/renderTarget.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/screenCapture.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/shader.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/textureGpu.o \
				TheForceEngine/TFE_RenderBackend/Win32OpenGL/vertexBuffer.o

SETTINGS	=	TheForceEngine/TFE_Settings/settings.o

SYSTEM		=	TheForceEngine/TFE_System/log.o \
				TheForceEngine/TFE_System/math.o \
				TheForceEngine/TFE_System/memoryPool.o \
				TheForceEngine/TFE_System/parser.o \
				TheForceEngine/TFE_System/profiler.o \
				TheForceEngine/TFE_System/system.o \
				TheForceEngine/TFE_System/Threads/Linux/mutexLinux.o \
				TheForceEngine/TFE_System/Threads/Linux/threadLinux.o

UI			=	TheForceEngine/TFE_Ui/imGUI/imgui.o \
				TheForceEngine/TFE_Ui/imGUI/imgui_demo.o \
				TheForceEngine/TFE_Ui/imGUI/imgui_draw.o \
				TheForceEngine/TFE_Ui/imGUI/imgui_impl_opengl3.o \
				TheForceEngine/TFE_Ui/imGUI/imgui_impl_sdl.o \
				TheForceEngine/TFE_Ui/imGUI/imgui_widgets.o \
				TheForceEngine/TFE_Ui/markdown.o \
				TheForceEngine/TFE_Ui/ui.o

MAIN		=	TheForceEngine/main.o

OBJS		=	$(ARCHIVE) \
				$(ASSET) \
				$(AUDIO) \
				$(DARKFORCES) \
				$(FILESYSTEM) \
				$(FRONTENDUI) \
				$(GAME) \
				$(INPUT) \
				$(JEDI) \
				$(MEMORY) \
				$(OUTLAWS) \
				$(POLYGON) \
				$(POSTPROCESS) \
				$(RENDER) \
				$(SETTINGS) \
				$(SYSTEM) \
				$(UI) \
				$(MAIN)

$(BIN): $(OBJS)
	$(CXX) -o $(BIN) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN) $(OBJS)
