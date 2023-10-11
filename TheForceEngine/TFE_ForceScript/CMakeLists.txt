file(GLOB_RECURSE SOURCES "*.cpp" "*.c")

target_sources(tfe PRIVATE ${SOURCES})

# Add support for ARM calling conventions.
# Uncomment after testing on ARM.

#set(ARCHFLAGS "${CMAKE_SYSTEM_PROCESSOR}")
#if (APPLE AND NOT IOS)
#  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
#    set(ARCHFLAGS "aarch64")
#  endif ()
#endif ()

#if(${ARCHFLAGS} MATCHES "^arm")
#    enable_language(ASM)
#    if(CMAKE_ASM_COMPILER_WORKS)
#        set(SOURCES Angelscript/angelscript/source/as_callfunc_arm.cpp Angelscript/angelscript/source/as_callfunc_arm_gcc.S)
#        set_property(SOURCE Angelscript/angelscript/source/as_callfunc_arm_gcc.S APPEND PROPERTY COMPILE_FLAGS " -Wa,-mimplicit-it=always")
#    else()
#        message(FATAL ERROR "ARM target requires a working assembler")
#    endif()
#endif()

#if(${ARCHFLAGS} MATCHES "^aarch64")
#    enable_language(ASM)
#    if(CMAKE_ASM_COMPILER_WORKS)
#        if(NOT APPLE)
#            set(SOURCES Angelscript/angelscript/source/as_callfunc_arm64.cpp Angelscript/angelscript/source/as_callfunc_arm64_gcc.S)
#        else()
#            set(SOURCES Angelscript/angelscript/source/as_callfunc_arm64.cpp Angelscript/angelscript/source/as_callfunc_arm64_xcode.S)
#        endif()
#    else()
#        message(FATAL ERROR "ARM target requires a working assembler")
#    endif()
#endif()
