file(GLOB SOURCES "*.cpp")
target_sources(tfe PRIVATE ${SOURCES})

add_subdirectory(Win32OpenGL/)
