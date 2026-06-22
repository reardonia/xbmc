cmake_minimum_required(VERSION 3.15)
project(SwigBindingsBuilder)

# Self-contained native code generator for Kodi's Python bindings. tinyxml2 is
# vendored (Kodi's own XML library) so the tool needs only a host C++ compiler:
# no Java, no Groovy, no host Python.
set(SOURCES SwigBindingsBuilder.cpp
            Node.cpp
            Transform.cpp
            SwigTypeParser.cpp
            Helper.cpp
            PythonTools.cpp
            Typemaps.cpp
            Emit.cpp
            tinyxml2.cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(SwigBindingsBuilder ${SOURCES})

install(TARGETS SwigBindingsBuilder DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        RENAME "${APP_NAME_LC}-SwigBindingsBuilder")
