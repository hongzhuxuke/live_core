cmake_minimum_required(VERSION 2.8)
#工程名
PROJECT(vhall_media_core)

#工程文件中使用相对路径
SET(CMAKE_SUPPRESS_REGENERATION 1)
SET(CMAKE_USE_RELATIVE_PATHS ON)
include(${CMAKE_ROOT}/Modules/CMakeDetermineSystem.cmake)

SET(MEDIA_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR} )

MESSAGE(STATUS "current platform:" ${CMAKE_SYSTEM_NAME})

#设置库目录
if(CMAKE_SYSTEM_NAME MATCHES "Windows")

    MESSAGE(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: " ${CMAKE_HOST_SYSTEM_PROCESSOR})		
    if(CMAKE_CL_64)
	    MESSAGE(STATUS "HOST: " "WINDOWS_x86_64")			
	    SET(FFMPEG_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86_64/ffmpeg)
	    SET(X264_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86_64/x264)
        SET(KD_VOICE_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86_64/kd_voice)
		SET(VMAF_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86_64/vmaf)
	else()
	    MESSAGE(STATUS "HOST: " "WINDOWS_X86")			
	    SET(FFMPEG_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86/ffmpeg)
	    SET(X264_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86/x264)
        SET(KD_VOICE_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86/kd_voice)
		SET(VMAF_PATH ${MEDIA_CORE_DIR}/build/windows/libsx86/vmaf)
	endif()
    
	FILE(GLOB_RECURSE FFMPEG_STATIC ${FFMPEG_PATH}/lib/*.lib)
	FILE(GLOB_RECURSE X264_STATIC ${X264_PATH}/lib/*.lib)
    FILE(GLOB_RECURSE KD_VOICE_STATIC ${KD_VOICE_PATH}/libs/*.lib)
	FILE(GLOB VMAF_STATIC_RELEASE ${VMAF_PATH}/lib/*.lib)
	FILE(GLOB VMAF_STATIC_DEBUG ${VMAF_PATH}/lib/Debug/*.lib)

	FILE(GLOB_RECURSE FFMPEG_SHARE ${FFMPEG_PATH}/bin/*.dll)
	FILE(GLOB_RECURSE X264_SHARE ${X264_PATH}/lib/*.dll)
    FILE(GLOB_RECURSE KD_VOICE_SHARE ${KD_VOICE_PATH}/bin/*.dll)
	FILE(GLOB_RECURSE FONT_LIB ${KD_VOICE_PATH}/*.ttf)
	FILE(GLOB_RECURSE KD_CONFIG_FILE ${KD_VOICE_PATH}/msc/*.cfg)
	FILE(GLOB_RECURSE VMAF_MODEL_FILE ${MEDIA_CORE_DIR}/mediaquality/model/vmaf_v0.6.1.*)

	if(FFMPEG_SHARE) 
		MESSAGE(STATUS "FFMPEG_SHARE: " ${FFMPEG_SHARE} " finded\n")
	endif()
			
	if(FFMPEG_STATIC) 
		MESSAGE(STATUS "FFMPEG_STATIC: " ${FFMPEG_STATIC} " finded\n")
	endif()
					
	if(X264_SHARE) 
		MESSAGE(STATUS "X264_SHARE: " ${X264_SHARE} " finded\n")
	endif()
			
	if(X264_STATIC)
		MESSAGE(STATUS "X264_STATIC: " ${X264_STATIC} " finded\n")
	endif()

    if(KD_VOICE_SHARE)
        MESSAGE(STATUS "KD_VOICE_SHARE: " ${KD_VOICE_SHARE} " finded\n")
    endif()

    if(KD_VOICE_STATIC)
        MESSAGE(STATUS "KD_VOICE_STATIC: " ${KD_VOICE_STATIC} " finded\n")
    endif()
	
	if(VMAF_STATIC_DEBUG)
        MESSAGE(STATUS "VMAF_STATIC: " ${VMAF_STATIC_DEBUG} " debug version finded\n")
    endif()
	if(VMAF_STATIC_RELEASE)
        MESSAGE(STATUS "VMAF_STATIC: " ${VMAF_STATIC_RELEASE} " release version finded\n")
    endif()
	
else()

	SET(LIBRARY_PATH 	${CMAKE_CURRENT_SOURCE_DIR}/../../../../build/libdeps/build/)
	if(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64" OR
		${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64" OR
		${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "IA64")

			SET(FFMPEG_PATH ${MEDIA_CORE_DIR}/build/linux/libsx86_64/ffmpeg)
			SET(X264_PATH ${MEDIA_CORE_DIR}/build/linux/libsx86_64/x264)
			SET(VMAF_PATH ${MEDIA_CORE_DIR}/build/linux/libsx86_64/vmaf)
			
			FILE(GLOB_RECURSE FFMPEG_STATIC ${FFMPEG_PATH}/lib/*.a)
			FILE(GLOB_RECURSE FFMPEG_SHARE ${FFMPEG_PATH}/bin/*.so)
			
			FILE(GLOB_RECURSE X264_STATIC ${X264_PATH}/lib/*.a)			
			FILE(GLOB_RECURSE X264_SHARE ${X264_PATH}/lib/*.so)
			
			FILE(GLOB_RECURSE VMAF_STATIC ${VMAF_PATH}/lib/*.a)			
			FILE(GLOB_RECURSE VMAF_SHARE ${VMAF_PATH}/lib/*.so)
			FILE(GLOB_RECURSE VMAF_MODEL_FILE ${MEDIA_CORE_DIR}/mediaquality/model/vmaf_v0.6.1.*)
			
			#get pthread
			find_library(PTHREAD_LIB NAMES pthread pthread_nonshared PATHS /usr/lib64/
				/usr/local/lib64/)
			if(PTHREAD_LIB)
				MESSAGE(STATUS "PTHREAD_LIB: " ${PTHREAD_LIB} " finded\n")
			endif()	

			find_library(RT_LIB NAMES rt PATHS /usr/lib64/
				/usr/local/lib64/)
			if(RT_LIB)
				MESSAGE(STATUS "RT_LIB: " ${RT_LIB} " finded\n")
			endif()	
			
			if(FFMPEG_SHARE) 
				MESSAGE(STATUS "FFMPEG_SHARE: " ${FFMPEG_SHARE} " finded\n")
			endif()
			
			if(FFMPEG_STATIC) 
				MESSAGE(STATUS "FFMPEG_STATIC: " ${FFMPEG_STATIC} " finded\n")
			endif()
					
			if(X264_SHARE) 
				MESSAGE(STATUS "X264_SHARE: " ${X264_SHARE} " finded\n")
			endif()
			
			if(X264_STATIC)
				MESSAGE(STATUS "X264_STATIC: " ${X264_STATIC} " finded\n")
			endif()
			
			if(VMAF_SHARE) 
				MESSAGE(STATUS "VMAF_SHARE: " ${VMAF_SHARE} " finded\n")
			endif()
			
			if(VMAF_STATIC)
				MESSAGE(STATUS "VMAF_STATIC: " ${VMAF_STATIC} " finded\n")
			endif()
			
			if(VMAF_MODEL_FILE)
				MESSAGE(STATUS "VMAF_MODEL_FILE: " ${VMAF_MODEL_FILE} " finded\n")
			endif()
            
            list(APPEND LINUX_DEPS_LIB "dl" "z" "vpx")
            
			#	if(ZLIB_STATIC) 
			#	MESSAGE(STATUS "ZLIB_STATIC: " ${ZLIB_STATIC} " finded\n")
			#endif()

			#if(FAAC_STATIC) 
			#	MESSAGE(STATUS "FAAC_STATIC: " ${FAAC_STATIC} " finded\n")
			#endif()
			#MESSAGE(STATUS "HOST: " "LINUX_X86_64")
	
	else()
			MESSAGE(STATUS "HOST: " "LINUX_X86 not sourport now")
	endif()	
	ADD_DEFINITIONS(-DPOSIX)	
	ADD_DEFINITIONS(-D__STDC_CONSTANT_MACROS)
	set(ERIZO_CMAKE_CXX_FLAGS "${ERIZO_CMAKE_CXX_FLAGS}  -fPIC")
	set(ERIZO_CMAKE_C_FLAGS "${ERIZO_CMAKE_C_FLAGS}  -fPIC")
	MESSAGE("media core ERIZO_CMAKE_CXX_FLAGS: ${ERIZO_CMAKE_CXX_FLAGS}")
endif()

#设置头文件目录
include_directories(${FFMPEG_PATH}/include)
include_directories(${X264_PATH}/include)
include_directories(${VMAF_PATH}/include)
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	include_directories(${KD_VOICE_PATH}/include)
endif()


#add json
include_directories(${MEDIA_CORE_DIR}/3rdparty)
include_directories(${MEDIA_CORE_DIR}/3rdparty/libjingle)
include_directories(${MEDIA_CORE_DIR}/3rdparty/audiomixing)
include_directories(${MEDIA_CORE_DIR}/3rdparty/srslibrtmp)
include_directories(${MEDIA_CORE_DIR}/3rdparty/noisesuppression)
include_directories(${MEDIA_CORE_DIR}/3rdparty/videofilter)


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	include_directories(${MEDIA_CORE_DIR}/voicetransition)
endif()
include_directories(${MEDIA_CORE_DIR}/audioprocess)
include_directories(${MEDIA_CORE_DIR}/common)
include_directories(${MEDIA_CORE_DIR}/utility)
include_directories(${MEDIA_CORE_DIR}/encoder)
include_directories(${MEDIA_CORE_DIR}/muxers)
include_directories(${MEDIA_CORE_DIR}/os)
include_directories(${MEDIA_CORE_DIR}/api)
include_directories(${MEDIA_CORE_DIR}/rtmpplayer)
include_directories(${MEDIA_CORE_DIR}/rtmppush)
include_directories(${MEDIA_CORE_DIR}/demuxers)
include_directories(${MEDIA_CORE_DIR}/decoder)
include_directories(${MEDIA_CORE_DIR}/ratecontrol)
include_directories(${MEDIA_CORE_DIR}/mediaquality)


#设置输出文件路径
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

#设置选项
ADD_DEFINITIONS(-D__STDC_CONSTANT_MACROS)
if(MSVC)
	ADD_DEFINITIONS(-DUNICODE -D_UNICODE)
endif()

if(NOT WIN32)
	include(CheckCXXCompilerFlag)  
	CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)  
	CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)  
	if(COMPILER_SUPPORTS_CXX11)  
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")  
		message(STATUS "The compiler ${CMAKE_CXX_COMPILER} and flag -std=c++11")  
	elseif(COMPILER_SUPPORTS_CXX0X)  
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")  
		message(STATUS "The compiler ${CMAKE_CXX_COMPILER} and flag -std=c++0x")  
	else()  
		message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support.Please use a different C++ compiler.")  
	endif() 
endif()


#添加第三方库编译
add_subdirectory(${MEDIA_CORE_DIR}/3rdparty/srslibrtmp)
add_subdirectory(${MEDIA_CORE_DIR}/3rdparty/audiomixing)
add_subdirectory(${MEDIA_CORE_DIR}/3rdparty/libjingle)
add_subdirectory(${MEDIA_CORE_DIR}/3rdparty/json)
add_subdirectory(${MEDIA_CORE_DIR}/3rdparty/noisesuppression)
add_subdirectory(${MEDIA_CORE_DIR}/3rdparty/videofilter)


#copy x264 and ffmpeg ka_voice share,linux use static library
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	FILE(COPY ${FFMPEG_SHARE} ${X264_SHARE} ${KD_VOICE_SHARE} ${FONT_LIB} ${VMAF_MODEL_FILE} DESTINATION ${LIBRARY_OUTPUT_PATH})
	FILE(COPY ${KD_CONFIG_FILE} DESTINATION ${LIBRARY_OUTPUT_PATH}/msc/)
	list(APPEND LIBJINGLE_DEPS_LIB "Winmm" "Crypt32" "Secur32" "Wininet" "ws2_32" "Wsock32.lib" "iphlpapi" "msc")
endif()


#设置源文件路径
FILE(GLOB_RECURSE COMMON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/*.h ${CMAKE_CURRENT_SOURCE_DIR}/common/*.c*)
FILE(GLOB_RECURSE OS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/os/*.h ${CMAKE_CURRENT_SOURCE_DIR}/os/*.c*)
FILE(GLOB_RECURSE UTILITY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/utility/*.h ${CMAKE_CURRENT_SOURCE_DIR}/utility/*.c*)
FILE(GLOB_RECURSE MUXERS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/muxers/*.h ${CMAKE_CURRENT_SOURCE_DIR}/muxers/*.c*)
FILE(GLOB_RECURSE ENCODER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/encoder/*.h ${CMAKE_CURRENT_SOURCE_DIR}/encoder/*.c*)
FILE(GLOB_RECURSE AUDIO_PROCESS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/audioprocess/*.h ${CMAKE_CURRENT_SOURCE_DIR}/audioprocess/*.c*)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
FILE(GLOB_RECURSE VOICE_TRANSITION_SRC ${PROJECT_SOURCE_DIR}/voicetransition/*.h ${PROJECT_SOURCE_DIR}/voicetransition/*.c*)
endif()

FILE(GLOB_RECURSE MEDIA_QUALITY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/mediaquality/*.h ${CMAKE_CURRENT_SOURCE_DIR}/mediaquality/*.cpp)
FILE(GLOB_RECURSE RATE_CONTROL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/ratecontrol/*.h ${CMAKE_CURRENT_SOURCE_DIR}/ratecontrol/*.c*)
FILE(GLOB_RECURSE DEMUXERS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/demuxers/*.h  ${CMAKE_CURRENT_SOURCE_DIR}/demuxers/*.c*)
FILE(GLOB_RECURSE DECODER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/decoder/*.h ${CMAKE_CURRENT_SOURCE_DIR}/decoder/*.c*)

FILE(GLOB_RECURSE RTMPPUSH_SRC ${CMAKE_CURRENT_SOURCE_DIR}/rtmppush/*.h ${CMAKE_CURRENT_SOURCE_DIR}/rtmppush/*.c*)
FILE(GLOB_RECURSE RTMPPLAY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/rtmpplayer/*.h ${CMAKE_CURRENT_SOURCE_DIR}/rtmpplayer/*.c*)
FILE(GLOB_RECURSE LIVEAPI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/api/*.h ${CMAKE_CURRENT_SOURCE_DIR}/api/*.c*)

#设置文件子目录。
SOURCE_GROUP(common FILES ${COMMON_SRC})
SOURCE_GROUP(os FILES ${OS_SRC})
SOURCE_GROUP(utility FILES ${UTILITY_SRC})
SOURCE_GROUP(muxers FILES ${MUXERS_SRC})
SOURCE_GROUP(encoder FILES ${ENCODER_SRC})
SOURCE_GROUP(audioprocess FILES ${AUDIO_PROCESS_SRC})
SOURCE_GROUP(ratecontrol FILES ${RATE_CONTROL_SRC})
SOURCE_GROUP(rtmppush FILES ${RTMPPUSH_SRC})
SOURCE_GROUP(demuxers FILES ${DEMUXERS_SRC})
SOURCE_GROUP(decoder FILES ${DECODER_SRC})
SOURCE_GROUP(rtmpplay FILES ${RTMPPLAY_SRC})
SOURCE_GROUP(mediaquality FILES ${MEDIA_QUALITY_SRC})
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
SOURCE_GROUP(voicetransition FILES ${VOICE_TRANSITION_SRC})
endif()


if(CMAKE_SYSTEM_NAME MATCHES "Windows")
#voicetransition
list(APPEND VOICE_TRANSITION_LIB_SRC ${VOICE_TRANSITION_SRC} ${COMMON_SRC} ${UTILITY_SRC})
MESSAGE(STATUS "voice transition lib:\n " ${VOICE_TRANSITION_LIB_SRC})
add_library(vhall_voice_transition STATIC ${VOICE_TRANSITION_LIB_SRC})
set_target_properties(vhall_voice_transition PROPERTIES name "vhall_voice_transition")
target_link_libraries(vhall_voice_transition libjingle ${FFMPEG_STATIC} ${KD_VOICE_STATIC}  ${PTHREAD_LIB} ${RT_LIB})
endif()

#audioprocess
list(APPEND AUDIO_PROCESS_LIB_SRC ${AUDIO_PROCESS_SRC} ${COMMON_SRC} ${UTILITY_SRC})
MESSAGE(STATUS "audio process lib:\n " ${AUDIO_PROCESS_LIB_SRC})
add_library(vhall_audio_process STATIC ${AUDIO_PROCESS_LIB_SRC})
set_target_properties(vhall_audio_process PROPERTIES name "vhall_audio_process")
target_link_libraries(vhall_audio_process libjingle noise_suppression ${PTHREAD_LIB} ${RT_LIB})

#ratecontrol lib
list(APPEND RATE_CONTROL_LIB_SRC ${RATE_CONTROL_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
#source_group_by_dir(ENCODER_LIB_SRC)
MESSAGE(STATUS "rate_control lib:\n " ${RATE_CONTROL_LIB_SRC})
add_library(rate_control STATIC ${RATE_CONTROL_LIB_SRC})
set_target_properties(rate_control PROPERTIES name "rate_control")
target_link_libraries(rate_control json libjingle ${PTHREAD_LIB} ${RT_LIB})

#encoder lib
list(APPEND ENCODER_LIB_SRC ${ENCODER_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
#source_group_by_dir(ENCODER_LIB_SRC)
MESSAGE(STATUS "encodre lib:\n " ${ENCODER_LIB_SRC})
add_library(vhall_encoder STATIC ${ENCODER_LIB_SRC})
set_target_properties(vhall_encoder PROPERTIES name "vhall_encoder")
target_link_libraries(vhall_encoder rate_control json libjingle ${X264_STATIC} ${FFMPEG_STATIC} ${PTHREAD_LIB} ${RT_LIB})

#mediaquality lib
list(APPEND MEDIA_QUALITY_LIB_SRC ${MEDIA_QUALITY_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
#source_group_by_dir(MEDIA_QUALITY_LIB_SRC)
MESSAGE(STATUS "media quality lib:\n " ${MEDIA_QUALITY_LIB_SRC})
add_library(media_quality STATIC ${MEDIA_QUALITY_LIB_SRC})
set_target_properties(media_quality PROPERTIES name "media_quality")
if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
	target_link_libraries(media_quality vhall_encoder ${VMAF_STATIC_DEBUG})
else()
	target_link_libraries(media_quality vhall_encoder ${VMAF_STATIC_RELEASE})
endif()
	
#muxers lib
list(APPEND MUXERS_LIB_SRC ${MUXERS_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
#source_group_by_dir(MUXERS_LIB_SRC)
MESSAGE(STATUS "muxer lib:\n " ${MUXERS_LIB_SRC})
add_library(vhall_muxers STATIC ${MUXERS_LIB_SRC})
set_target_properties(vhall_muxers PROPERTIES name "vhall_muxers")
target_link_libraries(vhall_muxers rate_control json libjingle srs_librtmp ${PTHREAD_LIB} ${RT_LIB})	

#rtmp push lib
list(APPEND PUSH_LIB_SRC ${RTMPPUSH_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
MESSAGE(STATUS "push_srcs:\n " ${PUSH_LIB_SRC})
add_library(rtmp_push STATIC ${PUSH_LIB_SRC})
set_target_properties(rtmp_push PROPERTIES name "rtmp_push")
target_link_libraries(rtmp_push json libjingle media_quality vhall_audio_process vhall_encoder vhall_muxers ${PTHREAD_LIB} ${RT_LIB} )

#decoder lib
list(APPEND DECODER_LIB_SRC ${DECODER_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
MESSAGE(STATUS "decodre lib:\n " ${DECODER_LIB_SRC})
add_library(vhall_decoder STATIC ${DECODER_LIB_SRC})
set_target_properties(vhall_decoder PROPERTIES name "vhall_decoder")
target_link_libraries(vhall_decoder json libjingle ${X264_STATIC} ${FFMPEG_STATIC} ${PTHREAD_LIB} ${RT_LIB})

#demuxers lib
list(APPEND DEMUXERS_LIB_SRC ${DEMUXERS_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
MESSAGE(STATUS "demuxer lib:\n " ${DEMUXERS_LIB_SRC})
add_library(vhall_demuxers STATIC ${DEMUXERS_LIB_SRC})
set_target_properties(vhall_demuxers PROPERTIES name "vhall_demuxers")
target_link_libraries(vhall_demuxers json libjingle srs_librtmp ${PTHREAD_LIB} ${RT_LIB})	

#rtmp play lib
list(APPEND PLAY_LIB_SRC ${RTMPPLAY_SRC} ${COMMON_SRC} ${OS_SRC} ${UTILITY_SRC})
MESSAGE(STATUS "play_srcs:\n " ${PLAY_LIB_SRC})
add_library(rtmp_play STATIC ${PLAY_LIB_SRC})
set_target_properties(rtmp_play PROPERTIES name "rtmp_play")
target_link_libraries(rtmp_play json libjingle vhall_decoder vhall_demuxers ${FFMPEG_STATIC} ${PTHREAD_LIB} ${RT_LIB}  ${LINUX_DEPS_LIB} )


#rtmp push test
#add_executable(rtmp_push_test ${CMAKE_CURRENT_SOURCE_DIR}/test/rtmp_push_test.cc)
#target_link_libraries(rtmp_push_test rtmp_push ${LIBJINGLE_DEPS_LIB} ${FFMPEG_STATIC} ${PTHREAD_LIB} ${RT_LIB}  ${LINUX_DEPS_LIB} )
if(MSVC)
#	set_target_properties(rtmp_push_test PROPERTIES LINK_FLAGS "/SAFESEH:NO")
endif()

#rtmp play test
#add_executable(rtmp_play_test ${CMAKE_CURRENT_SOURCE_DIR}/test/rtmp_play_test.cc)
#target_link_libraries(rtmp_play_test rtmp_play ${LIBJINGLE_DEPS_LIB} ${LINUX_DEPS_LIB}  ${FFMPEG_STATIC})
if(MSVC)
#	set_target_properties(rtmp_play_test PROPERTIES LINK_FLAGS "/SAFESEH:NO")
endif()

#encoder test
#add_executable(encoder_test ${CMAKE_CURRENT_SOURCE_DIR}/test/encoder_test.cc)
#target_link_libraries(encoder_test vhall_encoder)
if(MSVC)
#	set_target_properties(encoder_test PROPERTIES LINK_FLAGS "/SAFESEH:NO")
endif()

#media quality test
#add_executable(media_quality_test ${CMAKE_CURRENT_SOURCE_DIR}/test/media_quality_test.cc)
#target_link_libraries(media_quality_test media_quality)
if(MSVC)
#	set_target_properties(media_quality_test PROPERTIES LINK_FLAGS "/SAFESEH:NO")
endif()

#encode with mqa test
#add_executable(encode_with_mqa_test ${CMAKE_CURRENT_SOURCE_DIR}/test/encode_with_mqa_test.cc)
#target_link_libraries(encode_with_mqa_test rate_control media_quality)
if(MSVC)
#	set_target_properties(encode_with_mqa_test PROPERTIES LINK_FLAGS "/SAFESEH:NO")
endif()
