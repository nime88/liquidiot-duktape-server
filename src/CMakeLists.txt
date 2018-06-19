# Adding any directories
add_subdirectory (eventloop)
add_subdirectory (nodejs)
add_subdirectory (http)
add_subdirectory (logs)

# cpp flags
set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -std=c++11")
set ( CMAKE_C_FLAGS "-Wall -Wextra -lm -std=c99")

# adding libraries
add_library (Utility file_ops.cpp util.cpp)
add_library (JSApp applications_manager.cpp application.cpp)
add_library (HttpServer http_server.cpp)
add_library (Globals globals.c)

# making libraries accessable
target_link_libraries(Utility DukTape archive)
target_link_libraries(JSApp DukTape DukTapeExtra Http EventLoop NodeJS Logs)
target_link_libraries(HttpServer JSApp Http websockets)

target_include_directories (Utility PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (JSApp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (HttpServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


