# liquidiot-duktape-server

Requirements

- duktape 2.2.1

- libwebsockets version 3.0 stable

- libarchive latest realease (3.2.2)

Duktape Configure

python2 tools/configure.py --output-directory duktape-src -UDUK_USE_GLOBAL_BINDING -UDUK_USE_FATAL_HANDLER -UDUK_USE_VERBOSE_ERRORS
