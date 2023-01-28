file(GLOB SOURCES "*.cpp")
target_sources(tfe PRIVATE ${SOURCES})

if(WIN32)
	file(GLOB SRCWINTHRD "${CMAKE_CURRENT_SOURCE_DIR}/Threads/Win32/*.cpp")
	target_sources(tfe PRIVATE ${SRCWINTHRD}
			"${CMAKE_CURRENT_SOURCE_DIR}/CrashHandler/crashHandlerWin32.cpp"
	)
elseif(LINUX)
	file(GLOB SRCLNXTHRD "${CMAKE_CURRENT_SOURCE_DIR}/Threads/Linux/*.cpp")
	target_sources(tfe PRIVATE ${SRCLNXTHRD}
		"${CMAKE_CURRENT_SOURCE_DIR}/CrashHandler/crashHandlerLinux.cpp"
	)
endif()
