# adding library
add_library (EventLoop custom_eventloop.cpp)

# linking libraries
target_link_libraries(EventLoop DukTape DukTapeExtra JSApp Globals Logs)

# making library accessable
target_include_directories (EventLoop PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
