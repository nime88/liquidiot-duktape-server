# cpp flags
set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -std=c++11")

# adding library
add_library (Http http_request.cpp get_request.cpp)

# linking libraries
target_link_libraries(Http JSApp websockets)

# making library accessable
target_include_directories (Http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

