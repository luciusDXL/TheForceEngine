file(GLOB SOURCES "*.cpp")
target_sources(tfe PRIVATE ${SOURCES})

if(WIN32)
	file(GLOB SRCPLAT "${CMAKE_CURRENT_SOURCE_DIR}/windows/*.cpp")
elseif(LINUX)
	file(GLOB SRCPLAT "${CMAKE_CURRENT_SOURCE_DIR}/linux/*.cpp")
endif()
target_sources(tfe PRIVATE ${SRCPLAT})
