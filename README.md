# liquidiot-duktape-server

Requirements

- duktape 2.2.1

- libwebsockets version 3.0 stable

- libarchive latest realease (3.2.2)

Duktape Configure

python2 tools/configure.py --output-directory duktape-src -UDUK_USE_GLOBAL_BINDING -UDUK_USE_FATAL_HANDLER -UDUK_USE_VERBOSE_ERRORS

Install

CMake assumes that duktape is located at "../duktape-2.2.1/" so one might want to look at the path if that's not the case.

- mkdir build && cd build

- cmake ..

- make

Currently 'make install' doesn't do anything more as this still is very much in development
