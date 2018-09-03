# adding libraries
add_library (ModDukTape mod_duk_console.h mod_duk_console.cpp 
	duk_util.h duk_util.cpp 
	duktape_interface.h duktape_interface.cpp 
	duktape_callbacks.h duktape_callbacks.cpp)

# linking necessary dependencies
target_link_libraries (ModDukTape DukTape Constant Utility JSApp Logs NodeJS)

# making libraries accessable
target_include_directories (ModDukTape PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

