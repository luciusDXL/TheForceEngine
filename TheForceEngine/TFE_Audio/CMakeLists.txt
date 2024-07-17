file(GLOB_RECURSE SOURCES "*.cpp" "*.c")
if(LINUX)
	# we use the system libraries on Linux
	list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/RtMidi.cpp")
endif()
if(NOT ENABLE_SYSMIDI)
	list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/systemMidiDevice.cpp")
	if(WIN32)
		list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/RtMidi.cpp")
	endif()
endif()

target_sources(tfe PRIVATE ${SOURCES})
