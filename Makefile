CC	=	gcc
CXX	=	g++
LD	=	g++
# -O2 and higher cause random segfaults.
CXXFLAGS=	-O1 -ggdb3 -gdwarf-5 -pipe -I./TheForceEngine -I/usr/include/SDL2 -I/usr/include/rtaudio -I/usr/include/rtmidi
LDFLAGS	=	-lpthread -lGL -lGLEW -lSDL2 -lIL -lILU -lrtaudio -lrtmidi -lasound

ARCH	=	TheForceEngine/TFE_Archive/archive.o		\
		TheForceEngine/TFE_Archive/gobArchive.o		\
		TheForceEngine/TFE_Archive/gobMemoryArchive.o	\
		TheForceEngine/TFE_Archive/labArchive.o		\
		TheForceEngine/TFE_Archive/lfdArchive.o		\
		TheForceEngine/TFE_Archive/zipArchive.o		\
		TheForceEngine/TFE_Archive/zip/zip.o

ASS	=	TheForceEngine/TFE_Asset/assetSystem.o		\
		TheForceEngine/TFE_Asset/colormapAsset.o	\
		TheForceEngine/TFE_Asset/dfKeywords.o		\
		TheForceEngine/TFE_Asset/fontAsset.o		\
		TheForceEngine/TFE_Asset/gameMessages.o		\
		TheForceEngine/TFE_Asset/gifWriter.o		\
		TheForceEngine/TFE_Asset/gmidAsset.o		\
		TheForceEngine/TFE_Asset/imageAsset.o		\
		TheForceEngine/TFE_Asset/levelList.o		\
		TheForceEngine/TFE_Asset/levelObjectsAsset.o	\
		TheForceEngine/TFE_Asset/modelAsset.o		\
		TheForceEngine/TFE_Asset/modelAsset_jedi.o	\
		TheForceEngine/TFE_Asset/paletteAsset.o		\
		TheForceEngine/TFE_Asset/spriteAsset.o		\
		TheForceEngine/TFE_Asset/spriteAsset_Jedi.o	\
		TheForceEngine/TFE_Asset/textureAsset.o		\
		TheForceEngine/TFE_Asset/vocAsset.o		\
		TheForceEngine/TFE_Asset/vueAsset.o

AUD	=	TheForceEngine/TFE_Audio/audioDevice.o		\
		TheForceEngine/TFE_Audio/audioSystem.o		\
		TheForceEngine/TFE_Audio/midiDevice.o		\
		TheForceEngine/TFE_Audio/midiPlayer.o

DF	=	TheForceEngine/TFE_DarkForces/Actor/actor.o			\
		TheForceEngine/TFE_DarkForces/Actor/actorSerialization.o	\
		TheForceEngine/TFE_DarkForces/Actor/animTables.o		\
		TheForceEngine/TFE_DarkForces/Actor/bobaFett.o			\
		TheForceEngine/TFE_DarkForces/Actor/dragon.o			\
		TheForceEngine/TFE_DarkForces/Actor/enemies.o			\
		TheForceEngine/TFE_DarkForces/Actor/exploders.o			\
		TheForceEngine/TFE_DarkForces/Actor/flyers.o			\
		TheForceEngine/TFE_DarkForces/Actor/mousebot.o			\
		TheForceEngine/TFE_DarkForces/Actor/phaseOne.o			\
		TheForceEngine/TFE_DarkForces/Actor/phaseThree.o		\
		TheForceEngine/TFE_DarkForces/Actor/phaseTwo.o			\
		TheForceEngine/TFE_DarkForces/Actor/scenery.o			\
		TheForceEngine/TFE_DarkForces/Actor/sewer.o			\
		TheForceEngine/TFE_DarkForces/Actor/troopers.o			\
		TheForceEngine/TFE_DarkForces/Actor/turret.o			\
		TheForceEngine/TFE_DarkForces/Actor/welder.o			\
		TheForceEngine/TFE_DarkForces/GameUI/agentMenu.o		\
		TheForceEngine/TFE_DarkForces/GameUI/delt.o			\
		TheForceEngine/TFE_DarkForces/GameUI/editBox.o			\
		TheForceEngine/TFE_DarkForces/GameUI/escapeMenu.o		\
		TheForceEngine/TFE_DarkForces/GameUI/menu.o			\
		TheForceEngine/TFE_DarkForces/GameUI/missionBriefing.o		\
		TheForceEngine/TFE_DarkForces/GameUI/pda.o			\
		TheForceEngine/TFE_DarkForces/GameUI/uiDraw.o			\
		TheForceEngine/TFE_DarkForces/Landru/cutscene.o			\
		TheForceEngine/TFE_DarkForces/Landru/cutsceneList.o		\
		TheForceEngine/TFE_DarkForces/Landru/cutscene_film.o		\
		TheForceEngine/TFE_DarkForces/Landru/cutscene_player.o		\
		TheForceEngine/TFE_DarkForces/Landru/lactor.o			\
		TheForceEngine/TFE_DarkForces/Landru/lactorAnim.o		\
		TheForceEngine/TFE_DarkForces/Landru/lactorCust.o		\
		TheForceEngine/TFE_DarkForces/Landru/lactorDelt.o		\
		TheForceEngine/TFE_DarkForces/Landru/lcanvas.o			\
		TheForceEngine/TFE_DarkForces/Landru/ldraw.o			\
		TheForceEngine/TFE_DarkForces/Landru/lfade.o			\
		TheForceEngine/TFE_DarkForces/Landru/lfont.o			\
		TheForceEngine/TFE_DarkForces/Landru/lmusic.o			\
		TheForceEngine/TFE_DarkForces/Landru/lpalette.o			\
		TheForceEngine/TFE_DarkForces/Landru/lrect.o			\
		TheForceEngine/TFE_DarkForces/Landru/lsound.o			\
		TheForceEngine/TFE_DarkForces/Landru/lsystem.o			\
		TheForceEngine/TFE_DarkForces/Landru/ltimer.o			\
		TheForceEngine/TFE_DarkForces/Landru/lview.o			\
		TheForceEngine/TFE_DarkForces/Landru/textCrawl.o		\
		TheForceEngine/TFE_DarkForces/agent.o				\
		TheForceEngine/TFE_DarkForces/animLogic.o			\
		TheForceEngine/TFE_DarkForces/automap.o				\
		TheForceEngine/TFE_DarkForces/briefingList.o			\
		TheForceEngine/TFE_DarkForces/cheats.o				\
		TheForceEngine/TFE_DarkForces/config.o				\
		TheForceEngine/TFE_DarkForces/darkForcesMain.o			\
		TheForceEngine/TFE_DarkForces/gameMusic.o			\
		TheForceEngine/TFE_DarkForces/gameMessage.o			\
		TheForceEngine/TFE_DarkForces/generator.o			\
		TheForceEngine/TFE_DarkForces/hitEffect.o			\
		TheForceEngine/TFE_DarkForces/hud.o				\
		TheForceEngine/TFE_DarkForces/item.o				\
		TheForceEngine/TFE_DarkForces/logic.o				\
		TheForceEngine/TFE_DarkForces/mission.o				\
		TheForceEngine/TFE_DarkForces/pickup.o				\
		TheForceEngine/TFE_DarkForces/player.o				\
		TheForceEngine/TFE_DarkForces/playerCollision.o			\
		TheForceEngine/TFE_DarkForces/projectile.o			\
		TheForceEngine/TFE_DarkForces/random.o				\
		TheForceEngine/TFE_DarkForces/sound.o				\
		TheForceEngine/TFE_DarkForces/time.o				\
		TheForceEngine/TFE_DarkForces/updateLogic.o			\
		TheForceEngine/TFE_DarkForces/util.o				\
		TheForceEngine/TFE_DarkForces/vueLogic.o			\
		TheForceEngine/TFE_DarkForces/weapon.o				\
		TheForceEngine/TFE_DarkForces/weaponFireFunc.o

FS	=	TheForceEngine/TFE_FileSystem/filestream-posix.o	\
		TheForceEngine/TFE_FileSystem/fileutil-posix.o		\
		TheForceEngine/TFE_FileSystem/filewriterAsync.o		\
		TheForceEngine/TFE_FileSystem/memorystream.o		\
		TheForceEngine/TFE_FileSystem/paths-posix.o

FE	=	TheForceEngine/TFE_FrontEndUI/console.o			\
		TheForceEngine/TFE_FrontEndUI/editorTexture.o		\
		TheForceEngine/TFE_FrontEndUI/frontEndUi.o		\
		TheForceEngine/TFE_FrontEndUI/modLoader.o		\
		TheForceEngine/TFE_FrontEndUI/profilerView.o

GAME	=	TheForceEngine/TFE_Game/igame.o				\
		TheForceEngine/TFE_Game/reticle.o			\
		TheForceEngine/TFE_Game/saveSystem.o 

INPUT	=	TheForceEngine/TFE_Input/input.o			\
		TheForceEngine/TFE_Input/inputMapping.o

JEDI	=	TheForceEngine/TFE_Jedi/Collision/collision.o		\
		TheForceEngine/TFE_Jedi/IMuse/imConst.o			\
		TheForceEngine/TFE_Jedi/IMuse/imDigitalSound.o		\
		TheForceEngine/TFE_Jedi/IMuse/imList.o			\
		TheForceEngine/TFE_Jedi/IMuse/imMidiCmd.o		\
		TheForceEngine/TFE_Jedi/IMuse/imMidiPlayer.o		\
		TheForceEngine/TFE_Jedi/IMuse/imSoundFader.o		\
		TheForceEngine/TFE_Jedi/IMuse/imTrigger.o		\
		TheForceEngine/TFE_Jedi/IMuse/imuse.o			\
		TheForceEngine/TFE_Jedi/IMuse/midiData.o		\
		TheForceEngine/TFE_Jedi/InfSystem/infState.o		\
		TheForceEngine/TFE_Jedi/InfSystem/infSystem.o		\
		TheForceEngine/TFE_Jedi/InfSystem/message.o		\
		TheForceEngine/TFE_Jedi/Level/level.o			\
		TheForceEngine/TFE_Jedi/Level/levelData.o		\
		TheForceEngine/TFE_Jedi/Level/levelTextures.o		\
		TheForceEngine/TFE_Jedi/Level/rfont.o			\
		TheForceEngine/TFE_Jedi/Level/robject.o			\
		TheForceEngine/TFE_Jedi/Level/robjData.o		\
		TheForceEngine/TFE_Jedi/Level/roffscreenBuffer.o	\
		TheForceEngine/TFE_Jedi/Level/rsector.o			\
		TheForceEngine/TFE_Jedi/Level/rtexture.o		\
		TheForceEngine/TFE_Jedi/Level/rwall.o			\
		TheForceEngine/TFE_Jedi/Math/core_math.o		\
		TheForceEngine/TFE_Jedi/Math/cosTable.o			\
		TheForceEngine/TFE_Jedi/Memory/allocator.o		\
		TheForceEngine/TFE_Jedi/Memory/list.o			\
		TheForceEngine/TFE_Jedi/Renderer/jediRenderer.o		\
		TheForceEngine/TFE_Jedi/Renderer/rcommon.o		\
		TheForceEngine/TFE_Jedi/Renderer/rscanline.o		\
		TheForceEngine/TFE_Jedi/Renderer/rsectorRender.o	\
		TheForceEngine/TFE_Jedi/Renderer/screenDraw.o		\
		TheForceEngine/TFE_Jedi/Renderer/virtualFramebuffer.o	\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixed.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixedSharedState.o	\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/redgePairFixed.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rflatFixed.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rlightingFixed.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rsectorFixed.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/rwallFixed.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_Clipping.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_Culling.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_PolygonDraw.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_PolygonSetup.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Fixed/robj3d_fixed/robj3dFixed_TransformAndLighting.o	\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rclassicFloat.o					\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rclassicFloatSharedState.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/redgePairFloat.o				\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rflatFloat.o					\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rlightingFloat.o				\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rsectorFloat.o					\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/rwallFloat.o					\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_Clipping.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_Culling.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_PolygonDraw.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_PolygonSetup.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_Float/robj3d_float/robj3dFloat_TransformAndLighting.o	\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/debug.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/frustum.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/modelGPU.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/objectPortalPlanes.o	\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/rclassicGPU.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/renderDebug.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/rsectorGPU.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/sbuffer.o			\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/screenDrawGPU.o		\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/sectorDisplayList.o	\
		TheForceEngine/TFE_Jedi/Renderer/RClassic_GPU/spriteDisplayList.o	\
		TheForceEngine/TFE_Jedi/Serialization/serialization.o			\
		TheForceEngine/TFE_Jedi/Task/task.o

MEM	=	TheForceEngine/TFE_Memory/chunkedArray.o			\
		TheForceEngine/TFE_Memory/memoryRegion.o

OL	=	TheForceEngine/TFE_Outlaws/outlawsMain.o

POLY	=	TheForceEngine/TFE_Polygon/clipper.o				\
		TheForceEngine/TFE_Polygon/polygon.o

PP	=	TheForceEngine/TFE_PostProcess/blit.o				\
		TheForceEngine/TFE_PostProcess/overlay.o			\
		TheForceEngine/TFE_PostProcess/postprocess.o

RENDER	=	TheForceEngine/TFE_RenderShared/lineDraw2d.o			\
		TheForceEngine/TFE_RenderShared/quadDraw2d.o			\
		TheForceEngine/TFE_RenderShared/texturePacker.o 		\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/dynamicTexture.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/glslParser.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/indexBuffer.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/openGL_Caps.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/renderBackend.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/renderState.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/renderTarget.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/screenCapture.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/shader.o		\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/shaderBuffer.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/textureGpu.o	\
		TheForceEngine/TFE_RenderBackend/Win32OpenGL/vertexBuffer.o

SETT	=	TheForceEngine/TFE_Settings/settings.o				\
		TheForceEngine/TFE_Settings/linux/defpaths.o

SYS	=	TheForceEngine/TFE_System/log.o					\
		TheForceEngine/TFE_System/math.o				\
		TheForceEngine/TFE_System/memoryPool.o				\
		TheForceEngine/TFE_System/parser.o				\
		TheForceEngine/TFE_System/profiler.o				\
		TheForceEngine/TFE_System/system.o				\
		TheForceEngine/TFE_System/tfeMessage.o				\
		TheForceEngine/TFE_System/CrashHandler/crashHandlerLinux.o	\
		TheForceEngine/TFE_System/Threads/Linux/mutexLinux.o		\
		TheForceEngine/TFE_System/Threads/Linux/threadLinux.o

UI	=	TheForceEngine/TFE_Ui/imGUI/imgui.o				\
		TheForceEngine/TFE_Ui/imGUI/imgui_draw.o			\
		TheForceEngine/TFE_Ui/imGUI/imgui_impl_opengl3.o		\
		TheForceEngine/TFE_Ui/imGUI/imgui_impl_sdl.o			\
		TheForceEngine/TFE_Ui/imGUI/imgui_widgets.o			\
		TheForceEngine/TFE_Ui/markdown.o				\
		TheForceEngine/TFE_Ui/ui.o

MAIN	=	TheForceEngine/main.o

OBJS	=	$(ARCH)		\
		$(ASS)		\
		$(AUD)		\
		$(DF)		\
		$(FS)		\
		$(FE)		\
		$(GAME)		\
		$(INPUT)	\
		$(JEDI)		\
		$(MEM)		\
		$(OL)		\
		$(POLY)		\
		$(PP)		\
		$(RENDER)	\
		$(SETT)		\
		$(SYS)		\
		$(UI)		\
		$(MAIN)

BIN	=	TheForceEngine/tfelnx

$(BIN): $(OBJS)
	$(LD) -o $(BIN) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BIN) $(OBJS)
	find -name "*.o" -delete
