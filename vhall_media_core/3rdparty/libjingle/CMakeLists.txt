#cmake_minimum_required(VERSION 3.7)

#PROJECT(libjingle)
#set(PROJECT_VERSION_MAJOR 2)
#set(PROJECT_VERSION_MINOR 0)

#include(${PROJECT_SOURCE_DIR}/cmake/CheckCompiler.cmake)
#include(${PROJECT_SOURCE_DIR}/cmake/UserOption.cmake)

include(${CMAKE_ROOT}/Modules/CMakeDetermineSystem.cmake)
#if(CMAKE_SYSTEM_NAME == "Windows")
#	add_definitions(_WIN32)
if(WIN32)
	ADD_DEFINITIONS(-DWIN32)
else()
	if(UNIX)
		ADD_DEFINITIONS(-DLINUX)
		ADD_DEFINITIONS(-DPOSIX)
	endif()
endif()

ADD_SUBDIRECTORY(talk/base)
