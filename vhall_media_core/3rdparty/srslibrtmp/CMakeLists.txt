
include(${CMAKE_ROOT}/Modules/CMakeDetermineSystem.cmake)
if(WIN32)
	ADD_DEFINITIONS(-D_WIN32)
endif()

include_directories(./src/core ./src/kernel ./src/libs ./src/protocol ./objs ./multitcp)

ADD_SUBDIRECTORY(./multitcp)
ADD_SUBDIRECTORY(./src)


