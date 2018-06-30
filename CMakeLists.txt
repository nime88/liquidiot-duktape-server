cmake_minimum_required (VERSION 3.0.2)
project (LIQUID_IOT_SERVER)

## INSTALL ##

# creating vecessary directories
install(DIRECTORY DESTINATION ${LIQUID_IOT_SERVER_BINARY_DIR}/applications)
install(DIRECTORY DESTINATION ${LIQUID_IOT_SERVER_BINARY_DIR}/tmp)
#install(DIRECTORY DESTINATION ${LIQUID_IOT_SERVER_BINARY_DIR}/lib)

## BUILD ##

# Adding source directory
add_subdirectory (src)

# Adding duktape
set( DUKTAPE_PATH "../duktape-2.2.1/src" )
add_library(DukTape ${DUKTAPE_PATH}/duktape.c)
target_include_directories(DukTape PUBLIC ${LIQUID_IOT_SERVER_SOURCE_DIR}/${DUKTAPE_PATH})

set( DUKTAPE_EXTRAS_DIR "../duktape-2.2.1/extras/")
set( DUKTAPE_MODULE_NODE "${LIQUID_IOT_SERVER_SOURCE_DIR}/${DUKTAPE_EXTRAS_DIR}/module-node")
set( DUKTAPE_PRINT_ALERT "${LIQUID_IOT_SERVER_SOURCE_DIR}/${DUKTAPE_EXTRAS_DIR}/print-alert")
add_library(DukTapeExtra ${DUKTAPE_MODULE_NODE}/duk_module_node.c ${DUKTAPE_PRINT_ALERT}/duk_print_alert.c)
target_link_libraries(DukTapeExtra DukTape)
target_include_directories(DukTapeExtra PUBLIC ${DUKTAPE_MODULE_NODE} ${DUKTAPE_PRINT_ALERT} ${DUKTAPE_CONSOLE})

# Adding external libraries
#add_library(websockets SHARED IMPORTED)
#add_library(archive SHARED IMPORTED)

# Setting up some compiler stuff

set ( CMAKE_C_FLAGS "-Wall -Wextra -lm -std=c99")
set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -pthread -std=c++11 -lstdc++")

add_executable (liquid-server src/main.cpp)

target_link_libraries (liquid-server LINK_PUBLIC HttpServer JSApp Device)

# Some post build file moving stuff to help debugging and stuff
add_custom_command(TARGET liquid-server POST_BUILD 
                   COMMAND ${CMAKE_COMMAND} -E make_directory
                       $<TARGET_FILE_DIR:liquid-server>/applications)
add_custom_command(TARGET liquid-server POST_BUILD 
                   COMMAND ${CMAKE_COMMAND} -E make_directory
                       $<TARGET_FILE_DIR:liquid-server>/tmp)
add_custom_command(TARGET liquid-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${LIQUID_IOT_SERVER_SOURCE_DIR}/lib $<TARGET_FILE_DIR:liquid-server>/lib)
add_custom_command(TARGET liquid-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${LIQUID_IOT_SERVER_SOURCE_DIR}/http-files $<TARGET_FILE_DIR:liquid-server>/http-files)
add_custom_command(TARGET liquid-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${LIQUID_IOT_SERVER_SOURCE_DIR}/js $<TARGET_FILE_DIR:liquid-server>/js)
add_custom_command(TARGET liquid-server POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${LIQUID_IOT_SERVER_SOURCE_DIR}/config.cfg $<TARGET_FILE_DIR:liquid-server>)
