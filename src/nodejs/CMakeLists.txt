# adding library
add_library (NodeJS node_module_search.cpp)

# making library accessable
target_link_libraries(NodeJS Utility)
target_include_directories (NodeJS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

