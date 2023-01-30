file(GLOB SOURCES "*.cpp")
target_sources(tfe PRIVATE ${SOURCES})
file(GLOB IMGUI "imGUI/*.cpp")
# remove the unneeded demo file
list(REMOVE_ITEM IMGUI "${CMAKE_CURRENT_SOURCE_DIR}/imGUI/imgui_demo.cpp")
target_sources(tfe PRIVATE ${IMGUI})
