#include <stdio.h>

#include "globals.h"

int interrupted;

void sigint_handler(int sig) {
  interrupted = 1;
}
