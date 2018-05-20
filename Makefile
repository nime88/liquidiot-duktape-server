# Using gcc as compiler version 5.4.0 has been tested (although it should also work on earlier compliler versions)
CC=gcc
SOURCE_DIR=src
DUKTAPE_DIR=../duktape-2.2.1
DUKTAPE_EXTRAS=$(DUKTAPE_DIR)/extras

CFLAGS=-Wall -Wextra -lstdc++ -lm
INCLUDE=-I$(SOURCE_DIR) -I$(DUKTAPE_DIR)/src -I$(DUKTAPE_EXTRAS)/print-alert
# TODO: Currently build directory is not automatically generated
BUILD_DIR=build

MAIN_DEPS = $(DUKTAPE_DIR)/src/duktape.h $(DUKTAPE_EXTRAS)/print-alert/duk_print_alert.h

all: duktape.o duk_print_alert.o file_ops.o main.o
	$(CC) -o liquid-server $(BUILD_DIR)/duk_print_alert.o $(BUILD_DIR)/duktape.o $(BUILD_DIR)/file_ops.o $(BUILD_DIR)/main.o $(CFLAGS)

main.o: $(SOURCE_DIR)/main.cpp $(MAIN_DEPS)
	$(CC) -c $(SOURCE_DIR)/main.cpp -o $(BUILD_DIR)/main.o $(INCLUDE) $(CFLAGS)

duktape.o: $(DUKTAPE_DIR)/src/duktape.c
	$(CC) -c -std=c99 $(DUKTAPE_DIR)/src/duktape.c -o $(BUILD_DIR)/duktape.o -I$(DUKTAPE_DIR)/src $(CFLAGS)

duk_print_alert.o: $(DUKTAPE_EXTRAS)/print-alert/duk_print_alert.c
	$(CC) -c -std=c99 $(DUKTAPE_EXTRAS)/print-alert/duk_print_alert.c -o $(BUILD_DIR)/duk_print_alert.o -I$(DUKTAPE_DIR)/src $(CFLAGS) 

file_ops.o: $(SOURCE_DIR)/file_ops.cpp
	$(CC) -c $(SOURCE_DIR)/file_ops.cpp -o $(BUILD_DIR)/file_ops.o $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(BUILD_DIR)/*.o
	rm -f liquid-server
