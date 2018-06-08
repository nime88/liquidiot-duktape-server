# Adding any directories
add_subdirectory (eventloop)
add_subdirectory (nodejs)

# cpp flags
set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -std=c++11")
set ( CMAKE_C_FLAGS "-Wall -Wextra -lm -std=c99")

# adding libraries
add_library (Main application.cpp applications_manager.cpp file_ops.cpp http_server.cpp response.cpp util.cpp)
add_library (Globals globals.c)

# making libraries accessable
target_link_libraries(Main DukTape DukTapeExtra NodeJS Globals)
target_include_directories (Main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


