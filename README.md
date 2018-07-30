# liquidiot-duktape-server

## Requirements

- gcc 5.4.0 or complier able to complile -std=c++11

- cmake 3.0.2

- duktape 2.2.1 http://duktape.org/duktape-2.2.1.tar.xz

- libwebsockets version 3.0 stable https://github.com/warmcat/libwebsockets

- libarchive release (3.2.2) https://github.com/libarchive/libarchive

- dirent.h is also required (should already be on _*nix_ systems)

- boost (only boost::filesystem is required, so about 1.55.0 version should be enough)

- unistd.h unfortunately currently locking the platform to linux systems

*Some of these requirements are not absolute lower boundaries but rather tested versions*

## Duktape Configure

**Duktape doesn't need to be complied separately** (it is compled with the program), thus only the files are required.

Inside duktape folder configure duktape.
```
python2 tools/configure.py --output-directory src -UDUK_USE_FATAL_HANDLER -UDUK_USE_VERBOSE_ERRORS -UDUK_USE_CPP_EXCEPTIONS
```
Duktape default path can be configured with env DUKTAPE_PATH and extras path can be controlled with evn DUKTAPE_EXTRAS_DIR. Paths to both duktape env variables are treated as relative paths.
*Default configure should work but it is not tested properly*

## Install

CMake assumes that duktape is located at "../duktape-2.2.1/" so one might want to look at the path if that's not the case. *Might change in later releases.*

```
mkdir build && cd build
cmake ..
make
```

*Currently 'make install' doesn't do anything extra*

## Configure

1. Copy examples/example_config.json `cp examples/example_config.json build/config.json`
2. Change all `hostname` and `port` parameters accordingly (these are mandatory)
3. Change `url` parameters (only used for visibility and might be needed to communicate from outside)
4. Everything else important is generated automatically (although only device id will be overwritten if not apppropriate)
5. All **string** parameters will be saved as **string** (only native configs can be other than string)
6. Only parameters used in example_config.json are natively used (although other parameters will be visible eg. `"device" : { "serial":"asdf" }` )

## Other

Check for **"examples/test"** and **"examples/test2"** for example applications that are using the features supported by current runtime.

The runtime should start properly even when configuration is not correct.

To enable debug prints one should use `-DCMAKE_BUILD_TYPE=Debug` in cmake flags.
