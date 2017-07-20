# Use CMAKE_CURRENT_LIST_DIR because we are including this file not the subdirectory.
target_sources( filesplitter PRIVATE "${CMAKE_CURRENT_LIST_DIR}/utilities.cpp" )
target_sources( filesplitter PRIVATE "${CMAKE_CURRENT_LIST_DIR}/tool.cpp" )
target_sources( filesplitter PRIVATE "${CMAKE_CURRENT_LIST_DIR}/filesplitter.cpp" )

