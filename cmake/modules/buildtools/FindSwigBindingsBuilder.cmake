#.rst:
# FindSwigBindingsBuilder
# -----------------------
# Finds the SwigBindingsBuilder
#
# If WITH_SWIGBINDINGSBUILDER is defined, it will be used as the location of an
# existing SwigBindingsBuilder binary to execute during the build. Useful when
# cross-compiling. The file must exist and be executable at configure time.
#
#
# This will define the following (imported) targets::
#
#   SwigBindingsBuilder::SwigBindingsBuilder   - The SwigBindingsBuilder executable

if(NOT TARGET SwigBindingsBuilder::SwigBindingsBuilder)

  include(cmake/scripts/common/ModuleHelpers.cmake)

  if(WITH_SWIGBINDINGSBUILDER)
    get_filename_component(_sbbpath ${WITH_SWIGBINDINGSBUILDER} ABSOLUTE)
    if(NOT IS_DIRECTORY ${_sbbpath})
      get_filename_component(_sbbpath ${_sbbpath} DIRECTORY)
    endif()
    find_program(SWIGBINDINGSBUILDER_EXECUTABLE
                 NAMES "${APP_NAME_LC}-SwigBindingsBuilder" SwigBindingsBuilder
                 HINTS ${_sbbpath}
                 NO_DEFAULT_PATH)
    if(NOT SWIGBINDINGSBUILDER_EXECUTABLE)
      message(FATAL_ERROR "Could not find 'SwigBindingsBuilder' executable in ${_sbbpath} supplied by -DWITH_SWIGBINDINGSBUILDER")
    endif()
  else()

    set(${CMAKE_FIND_PACKAGE_NAME}_MODULE_LC SwigBindingsBuilder)
    set(${${CMAKE_FIND_PACKAGE_NAME}_MODULE_LC}_LIB_TYPE native)
    set(${${CMAKE_FIND_PACKAGE_NAME}_MODULE_LC}_DISABLE_VERSION ON)
    SETUP_BUILD_VARS()

    # Override build type detection and always build as release
    set(SWIGBINDINGSBUILDER_BUILD_TYPE Release)

    unset(CMAKE_ARGS)
    if(ENABLE_CLANGTIDY)
      set(${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_LIST_SEPARATOR LIST_SEPARATOR |)
      string(REPLACE ";" "|" string_CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}")
      list(APPEND CMAKE_ARGS "-DCMAKE_CXX_CLANG_TIDY=${string_CMAKE_CXX_CLANG_TIDY}")
    else()
      list(APPEND CMAKE_ARGS -UCMAKE_CXX_CLANG_TIDY)
    endif()
    if(ENABLE_CPPCHECK)
      list(APPEND CMAKE_ARGS "-DCMAKE_CXX_CPPCHECK:FILEPATH=${CMAKE_CXX_CPPCHECK}")
    else()
      list(APPEND CMAKE_ARGS -UCMAKE_CXX_CPPCHECK)
    endif()
    if(ENABLE_INCLUDEWHATYOUUSE)
      list(APPEND CMAKE_ARGS "-DCMAKE_CXX_INCLUDE_WHAT_YOU_USE:FILEPATH=${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}")
    else()
      list(APPEND CMAKE_ARGS -UCMAKE_CXX_INCLUDE_WHAT_YOU_USE)
    endif()

    if(NATIVEPREFIX)
      set(INSTALL_DIR "${NATIVEPREFIX}/bin")
      set(SWIGBINDINGSBUILDER_INSTALL_PREFIX ${NATIVEPREFIX})
    else()
      set(INSTALL_DIR "${CMAKE_BINARY_DIR}/${CORE_BUILD_DIR}/bin")
      set(SWIGBINDINGSBUILDER_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/${CORE_BUILD_DIR})
    endif()

    # Set host build info for buildtool
    if(EXISTS "${NATIVEPREFIX}/share/Toolchain-Native.cmake")
      set(SWIGBINDINGSBUILDER_TOOLCHAIN_FILE "${NATIVEPREFIX}/share/Toolchain-Native.cmake")
    endif()

    if(WIN32 OR WINDOWS_STORE)
      # Make sure we generate for host arch, not target
      set(SWIGBINDINGSBUILDER_GENERATOR_PLATFORM CMAKE_GENERATOR_PLATFORM ${HOSTTOOLSET})
      set(APP_EXTENSION ".exe")
    endif()

    set(SWIGBINDINGSBUILDER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/tools/depends/native/SwigBindingsBuilder/src)
    set(SWIGBINDINGSBUILDER_EXECUTABLE ${INSTALL_DIR}/SwigBindingsBuilder${APP_EXTENSION})

    set(BUILD_BYPRODUCTS ${SWIGBINDINGSBUILDER_EXECUTABLE})

    BUILD_DEP_TARGET()

  endif()

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(SwigBindingsBuilder
                                    REQUIRED_VARS SWIGBINDINGSBUILDER_EXECUTABLE)

  if(SWIGBINDINGSBUILDER_FOUND)
    add_executable(SwigBindingsBuilder::SwigBindingsBuilder IMPORTED GLOBAL)
    set_target_properties(SwigBindingsBuilder::SwigBindingsBuilder PROPERTIES
                                                                   IMPORTED_LOCATION "${SWIGBINDINGSBUILDER_EXECUTABLE}")

    if(TARGET ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})
      add_dependencies(SwigBindingsBuilder::SwigBindingsBuilder ${${${CMAKE_FIND_PACKAGE_NAME}_MODULE}_BUILD_NAME})
    endif()
  else()
    if(SwigBindingsBuilder_FIND_REQUIRED)
      message(FATAL_ERROR "SwigBindingsBuilder not found.")
    endif()
  endif()

  mark_as_advanced(SWIGBINDINGSBUILDER_EXECUTABLE)
endif()
