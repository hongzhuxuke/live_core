FILE(GLOB_RECURSE ALL_SRC_LIST ./*.h ./*.c*)

FILE(GLOB_RECURSE WIN_SRC_LIST ./win*.h ./win*.c* ./schannel* ./*_win32.*)

FILE(GLOB_RECURSE MAC_SRC_LIST ./mac*.h ./mac*.c*)

FILE(GLOB_RECURSE LINUX_SRC_LIST ./linux*.h ./linux*.c* ./unix*.c* ./unix*.h ./posix*.h ./posix*.c* ./latebindingsymboltable* ./libdbusglibsymboltable* )


FILE(GLOB_RECURSE ALL_SRC_TEST ./*test*)
#FILE(GLOB_RECURSE SSL_SRC_TEST ./ssl*)
FILE(GLOB_RECURSE OPENSSL_SRC_TEST ./openssl*)
FILE(GLOB_RECURSE LINUX_WINDOWS_SRC_TEST ./linuxwindow*)

FILE(GLOB_RECURSE ERROR_SRC_TEST ./asyncpacketsocket* ./natserver* ./json* ./sslidentity.* ./sslstreamadapter.*)

list(REMOVE_ITEM ALL_SRC_LIST ${ALL_SRC_TEST} ${OPENSSL_SRC_TEST} ${ERROR_SRC_TEST})

if(WIN32)
	list(REMOVE_ITEM ALL_SRC_LIST ${LINUX_SRC_LIST} ${MAC_SRC_LIST})
else()
    if(MAC)
		list(REMOVE_ITEM ALL_SRC_LIST ${LINUX_SRC_LIST} ${WIN_SRC_LIST})
	else()
		list(REMOVE_ITEM ALL_SRC_LIST ${MAC_SRC_LIST} ${WIN_SRC_LIST})
		list(REMOVE_ITEM ALL_SRC_LIST ${LINUX_WINDOWS_SRC_TEST} ${WIN_SRC_LIST})
	endif()
endif()

include_directories(${PROJECT_SOURCE_DIR})
#set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/libs)
#source_group_by_dir(${SRC_LIST})
SOURCE_GROUP("libjingle\\talk\\base" FILES ${ALL_SRC_LIST})
ADD_LIBRARY(libjingle STATIC ${ALL_SRC_LIST})
#if(MSVC)
#use unicode
#	set_target_properties(libjingle PROPERTIES UNICODE _UNICODE)
	#set_target_properties( libjingle PROPERTIES COMPILE_FLAGS "/FAu")
#endif()

#if(CMAKE_HOST_WIN32)
#	MESSAGE(STATUS "srcs:\n " ${WINDOWS_SRC_LIST})
#	ADD_LIBRARY(libjingle STATIC ${WINDOWS_SRC_LIST})
#else()	
#	MESSAGE(STATUS "srcs:\n " ${LINUX_SRC_LIST})
#	ADD_LIBRARY(libjingle STATIC ${LINUX_SRC_LIST})
#endif()
