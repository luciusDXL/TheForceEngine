file(GLOB SOURCES "*.cpp")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/infAsset.cpp")
target_sources(tfe PRIVATE ${SOURCES})
