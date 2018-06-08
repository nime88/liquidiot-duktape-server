#include <stdio.h>

#include "globals.h"

int interrupted;

void sigint_handler(int sig) {
  (void)sig;
  interrupted = 1;
}
