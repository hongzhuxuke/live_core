#cmake版本要求
cmake_minimum_required(VERSION 2.8)
#设置工程名
#PROJECT(VideoFilter)
#工程文件中使用相对路径
#SET(CMAKE_SUPPRESS_REGENERATION 1)
#SET(CMAKE_USE_RELATIVE_PATHS ON)
#include(${CMAKE_ROOT}/Modules/CMakeDetermineSystem.cmake)

#设置头文件目录
include_directories(./src)
#设置输出文件路径
#SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
#SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

#设置选项
ADD_DEFINITIONS(-D__STDC_CONSTANT_MACROS)
if(MSVC)
	ADD_DEFINITIONS(-DUNICODE -D_UNICODE)
endif()

#设置源文件路径
FILE(GLOB_RECURSE SOURCE_CODE_FILE ./src/*.h ./src/*.cpp)

#设置文件子目录。
SOURCE_GROUP(video_filter FILES ${SOURCE_CODE_FILE})

#生成video_filter静态库

add_library(video_filter STATIC ${SOURCE_CODE_FILE})
#set_target_properties(video_filter PROPERTIES name "video_filter")

#生成example
 # add_executable(demo ./test/example.cpp)
 # target_link_libraries(demo video_filter)
 # if(MSVC)
	# set_target_properties(demo PROPERTIES LINK_FLAGS "/SAFESEH:NO")
 # endif()
 