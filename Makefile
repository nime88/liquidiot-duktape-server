# Using gcc as compiler version 5.4.0 has been tested (although it should also work on earlier compliler versions)
CC=gcc
SOURCE_DIR=src
DUKTAPE_DIR=../duktape-2.2.1
DUKTAPE_EXTRAS=$(DUKTAPE_DIR)/extras

LIBS = -lwebsockets
CFLAGS=-Wall -Wextra -lm
INCLUDE=-I$(SOURCE_DIR) -I$(SOURCE_DIR)/nodejs -I$(DUKTAPE_DIR)/src -I$(DUKTAPE_EXTRAS)/print-alert -I$(DUKTAPE_EXTRAS)/console -I$(DUKTAPE_EXTRAS)/module-node
# TODO: Currently build directory is not automatically generated
BUILD_DIR=build

_OBJ = duktape.o duk_print_alert.o duk_console.o duk_module_node.o c_eventloop.o http_server.o response.o node_module_search.o file_ops.o application.o applications_manager.o main.o
OBJ = $(patsubst %,$(BUILD_DIR)/%,$(_OBJ))

_DEPS = file_ops.h application.h applications_manager.h
DEPS = $(patsubst %,$(SOURCE_DIR)/%,$(_DEPS))

CPPVER = -lstdc++ -std=c++11

# build targets for local source
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(DEPS)
	$(CC) -c $< -o $@ $(INCLUDE) $(CPPVER) $(CFLAGS)

# full linking
all: $(OBJ)
	$(CC) -o liquid-server $^ $(CFLAGS) $(CPPVER) $(LIBS)

# internal but mostly separate libs
$(BUILD_DIR)/c_eventloop.o: $(SOURCE_DIR)/eventloop/c_eventloop.c
	$(CC) -c -std=c99 $(SOURCE_DIR)/eventloop/c_eventloop.c -o $(BUILD_DIR)/c_eventloop.o -I$(DUKTAPE_DIR)/src $(CFLAGS)

$(BUILD_DIR)/node_module_search.o: $(SOURCE_DIR)/nodejs/node_module_search.cpp
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPVER) $(INCLUDE)

# setting build targets for external libraries
$(BUILD_DIR)/duktape.o: $(DUKTAPE_DIR)/src/duktape.c
	$(CC) -c -std=c99 $(DUKTAPE_DIR)/src/duktape.c -o $(BUILD_DIR)/duktape.o -I$(DUKTAPE_DIR)/src $(CFLAGS)

$(BUILD_DIR)/duk_print_alert.o: $(DUKTAPE_EXTRAS)/print-alert/duk_print_alert.c
	$(CC) -c -std=c99 $(DUKTAPE_EXTRAS)/print-alert/duk_print_alert.c -o $(BUILD_DIR)/duk_print_alert.o -I$(DUKTAPE_DIR)/src $(CFLAGS)

$(BUILD_DIR)/duk_console.o: $(DUKTAPE_EXTRAS)/console/duk_console.c
	$(CC) -c -std=c99 $(DUKTAPE_EXTRAS)/console/duk_console.c -o $(BUILD_DIR)/duk_console.o -I$(DUKTAPE_DIR)/src $(CFLAGS)

$(BUILD_DIR)/duk_module_node.o: $(DUKTAPE_EXTRAS)/module-node/duk_module_node.c
	$(CC) -c -std=c99 $(DUKTAPE_EXTRAS)/module-node/duk_module_node.c -o $(BUILD_DIR)/duk_module_node.o -I$(DUKTAPE_DIR)/src $(CFLAGS)

# cleaning objects and executable
.PHONY: clean

clean:
	rm -f $(BUILD_DIR)/*.o
	rm -f *.o
	rm -f liquid-server
	rm -f $(SOURCE_DIR)/*.gch
