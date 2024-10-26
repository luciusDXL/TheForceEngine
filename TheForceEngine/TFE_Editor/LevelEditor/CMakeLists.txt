add_subdirectory(Rendering/)
add_subdirectory(Scripting/)
file(GLOB SOURCES "*.cpp")
target_sources(tfe PRIVATE ${SOURCES})
