#aux_source_directory(./core SRC_CORE_LIST)
#aux_source_directory(./kernel SRC_KERNEL_LIST)
#aux_source_directory(./libs SRC_LIB_LIST)
#aux_source_directory(./protocol SRC_PROTOCOL_LIST)

FILE(GLOB_RECURSE SRC_CORE_LIST ./core/*.*)
FILE(GLOB_RECURSE SRC_KERNEL_LIST ./kernel/*.*)
FILE(GLOB_RECURSE SRC_LIB_LIST ./libs/*.*)
FILE(GLOB_RECURSE SRC_PROTOCOL_LIST ./protocol/*.*)

FILE(GLOB_RECURSE SRC_OBJS_LIST ../objs/*.hpp)

FILE(GLOB_RECURSE SRC_OUTPUT_HEADER_LIST ../srs_librtmp.h)


SOURCE_GROUP("src\\core" FILES ${SRC_CORE_LIST})
SOURCE_GROUP("src\\kernel" FILES ${SRC_KERNEL_LIST})
SOURCE_GROUP("src\\libs" FILES ${SRC_LIB_LIST})
SOURCE_GROUP("src\\protocol" FILES ${SRC_PROTOCOL_LIST})
SOURCE_GROUP("objs" FILES ${SRC_OBJS_LIST})
SOURCE_GROUP("include" FILES ${SRC_OUTPUT_HEADER_LIST})

list(APPEND SRC_LIST ${SRC_CORE_LIST} ${SRC_KERNEL_LIST} ${SRC_LIB_LIST} ${SRC_PROTOCOL_LIST} ${SRC_OBJS_LIST} ${SRC_OUTPUT_HEADER_LIST})

ADD_LIBRARY(srs_librtmp STATIC ${SRC_LIST})
set_target_properties(srs_librtmp PROPERTIES name "srs_librtmp")
target_link_libraries(srs_librtmp multitcp)
