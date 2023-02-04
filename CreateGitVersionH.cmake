# based on https://chromium.googlesource.com/external/github.com/google/benchmark/+/refs/heads/main/cmake/GetGitVersion.cmake
#
# create a TFE-compatible "gitVersion.h" file in the build directory.
#
find_package(Git)

function(create_git_version_h)
  if(GIT_EXECUTABLE)
      execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --abbrev=8
          WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
          RESULT_VARIABLE status
          OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
          ERROR_QUIET)
      if(status)
          set(GIT_DESCRIBE_VERSION "v0.0.0")
      endif()

      string(STRIP ${GIT_DESCRIBE_VERSION} GIT_DESCRIBE_VERSION)

      # Work out if the repository is dirty
      execute_process(COMMAND ${GIT_EXECUTABLE} update-index -q --refresh
          WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
          OUTPUT_QUIET
          ERROR_QUIET)
      execute_process(COMMAND ${GIT_EXECUTABLE} diff-index --name-only HEAD --
          WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
          OUTPUT_VARIABLE GIT_DIFF_INDEX
          ERROR_QUIET)
      string(COMPARE NOTEQUAL "${GIT_DIFF_INDEX}" "" GIT_DIRTY)
      if (${GIT_DIRTY})
          set(GIT_DESCRIBE_VERSION "${GIT_DESCRIBE_VERSION}+")
      endif()
  else()
      set(GIT_DESCRIBE_VERSION "0.0.0")
  endif()
  message(STATUS "git version: ${GIT_DESCRIBE_VERSION}")
  set(VERSFILE ${CMAKE_CURRENT_BINARY_DIR}/gitVersion.h)
  #string(TIMESTAMP TODAY "%Y%m%d %H%M%S")
  set(VERSION "const char c_gitVersion[] = R\"(${GIT_DESCRIBE_VERSION} ${TODAY})\";")
  if(EXISTS ${VERSFILE})
      file(READ ${VERSFILE} VERSIONX)
  else()
      set(VERSIONX "")
  endif()
  if (NOT "${VERSION}" STREQUAL "${VERSIONX}")
      file(WRITE ${VERSFILE} "${VERSION}")
      message(STATUS "wrote new ${VERSFILE}")
  endif()
endfunction()
