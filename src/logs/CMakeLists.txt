# cpp flags
set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -std=c++11")

# adding library
add_library (Logs app_log.cpp read_app_log.cpp)

# linking libraries
target_link_libraries(Logs Constant)

# making library accessable
target_include_directories (Logs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

