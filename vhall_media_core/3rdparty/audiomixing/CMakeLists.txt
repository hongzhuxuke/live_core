#aux_source_directory(. SRC_LIST)
FILE(GLOB_RECURSE SRC_PROTOCOL_H ./ *.h)
FILE(GLOB_RECURSE SRC_PROTOCOL_C ./ *.cpp)

list(APPEND SRC_LIST ${SRC_PROTOCOL_H} ${SRC_PROTOCOL_C})

SOURCE_GROUP(audio_mixing FILES ${SRC_LIST})

include_directories("../")
#set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/libs)
#source_group_by_dir(${SRC_LIST})
ADD_LIBRARY(audio_mixing STATIC ${SRC_LIST})