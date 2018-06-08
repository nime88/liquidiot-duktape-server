set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -std=c++11")

# adding library
add_library (NodeJS node_module_search.cpp)

# making library accessable
target_link_libraries(NodeJS Main)
target_include_directories (NodeJS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

