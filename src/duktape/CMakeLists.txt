# adding libraries
add_library (ModDukTape mod_duk_console.cpp duk_util.cpp)

# linking necessary dependencies
target_link_libraries (ModDukTape DukTape Constant JSApp Logs)

# making libraries accessable
target_include_directories (ModDukTape PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

