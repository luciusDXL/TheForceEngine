file(GLOB SOURCES "*.cpp")
target_sources(tfe PRIVATE ${SOURCES})

if(WIN32)
	target_sources(tfe PRIVATE ${SRCWINTHRD}
			"${CMAKE_CURRENT_SOURCE_DIR}/CrashHandler/crashHandlerWin32.cpp"
	)
elseif(LINUX)
	target_sources(tfe PRIVATE ${SRCLNXTHRD}
		"${CMAKE_CURRENT_SOURCE_DIR}/CrashHandler/crashHandlerLinux.cpp"
	)
endif()
