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
add_library (Globals globals.cpp)
add_library (Device device.cpp)
add_library (Constant constant.cpp)

# linking necessary dependencies
target_link_libraries(Utility DukTape Constant archive)
target_link_libraries(JSApp DukTape DukTapeExtra Http EventLoop NodeJS Logs Device Utility)
target_link_libraries(HttpServer JSApp Http websockets)
target_link_libraries(Device DukTape Utility HttpClient)
target_link_libraries(Globals Device)

# making libraries accessable
target_include_directories (Utility PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (JSApp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (HttpServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (Globals PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (Device PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

