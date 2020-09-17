FILE(GLOB_RECURSE SRC_LIST ./*.h ./*.c*)
SOURCE_GROUP("multitcp" FILES ${SRC_LIST})
ADD_LIBRARY(multitcp STATIC ${SRC_LIST})
set_target_properties(multitcp PROPERTIES name "multitcp")