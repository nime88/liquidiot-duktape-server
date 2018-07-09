cmake_minimum_required (VERSION 3.0.2)
project (LIQUID_IOT_SERVER)

set(CMAKE_PROJECT_NAME "liquidiot-duktape-server")

SET(MAJOR_VERSION "0")
SET(MINOR_VERSION "1")
SET(PATCH_VERSION "1")

## BUILD ##
# cpp flags
set ( CMAKE_C_FLAGS "-Wall -Wextra -lm -std=c99" )
set ( CMAKE_CXX_FLAGS "-Wall -Wextra -lm -pthread -std=c++11 -lstdc++" )
set ( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG_PRINT" )
set ( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG_PRINT" )
set ( CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3" )
set ( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3" )

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
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost COMPONENTS filesystem)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

# Setting up some compiler stuff

add_executable (liquid-server src/main.cpp)
target_link_libraries (liquid-server LINK_PUBLIC HttpServer JSApp Device ${Boost_LIBRARIES})

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
                       ${LIQUID_IOT_SERVER_SOURCE_DIR}/examples/example_config.json $<TARGET_FILE_DIR:liquid-server>/config.json)

## INSTALL ##

# creating necessary directories
install(DIRECTORY DESTINATION liquidiot-duktape-server)
install(TARGETS liquid-server DESTINATION liquidiot-duktape-server)
install(DIRECTORY DESTINATION liquidiot-duktape-server/applications DIRECTORY_PERMISSIONS
  OWNER_WRITE OWNER_READ OWNER_EXECUTE
  GROUP_WRITE GROUP_READ GROUP_EXECUTE
  WORLD_WRITE WORLD_READ WORLD_EXECUTE)
install(DIRECTORY DESTINATION liquidiot-duktape-server/tmp DIRECTORY_PERMISSIONS
  OWNER_WRITE OWNER_READ OWNER_EXECUTE
  GROUP_WRITE GROUP_READ GROUP_EXECUTE
  WORLD_WRITE WORLD_READ WORLD_EXECUTE)
install(DIRECTORY ${LIQUID_IOT_SERVER_SOURCE_DIR}/lib DESTINATION liquidiot-duktape-server)
install(DIRECTORY ${LIQUID_IOT_SERVER_SOURCE_DIR}/http-files DESTINATION liquidiot-duktape-server)
install(DIRECTORY ${LIQUID_IOT_SERVER_SOURCE_DIR}/js DESTINATION liquidiot-duktape-server)
install(FILES ${LIQUID_IOT_SERVER_SOURCE_DIR}/examples/example_config.json DESTINATION liquidiot-duktape-server RENAME config.json PERMISSIONS
  OWNER_WRITE OWNER_READ OWNER_EXECUTE
  GROUP_WRITE GROUP_READ GROUP_EXECUTE
  WORLD_WRITE WORLD_READ WORLD_EXECUTE)

set(target "liquidiot-duktape-server/liquid-server")
set(link "bin/liquid-server")
install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${CMAKE_INSTALL_PREFIX}/${target} \
    ${CMAKE_INSTALL_PREFIX}/${link}   \
    )"
)

## cpack stuff

SET(CPACK_SET_DESTDIR "on")
SET(CPACK_PACKAGING_INSTALL_PREFIX "/tmp")
SET(CPACK_GENERATOR "DEB")

SET(CPACK_PACKAGE_CONTACT "oiva.moisio@student.tut.fi")
SET(CPACK_PACKAGE_DESCRIPTION "Lightweight liquidiot runtime.")
SET(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")
SET(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${MAJOR_VERSION}.${MINOR_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${MAJOR_VERSION}.${MINOR_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")

# SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libwebsockets , libarchive ")

SET(CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})

SET(CPACK_COMPONENTS_ALL Libraries ApplicationData)
INCLUDE(CPack)
