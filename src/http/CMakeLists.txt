# cpp flags
set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -std=c++11")

# adding library
add_library (Http http_request.cpp get_request.cpp post_request.cpp delete_request.cpp put_request.cpp app_request.cpp app_response.cpp)
add_library (HttpClient http_client.cpp client_request_config.cpp)

# linking libraries
target_link_libraries(Http JSApp Utility websockets)
target_link_libraries(HttpClient DukTape Globals websockets)

# making library accessable
target_include_directories (Http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (HttpClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

