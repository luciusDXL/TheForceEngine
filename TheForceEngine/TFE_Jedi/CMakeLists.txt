file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")
# remove stray file laying around
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Renderer/RClassic_Float/screenDraw.cpp")
target_sources(tfe PRIVATE ${SOURCES})
