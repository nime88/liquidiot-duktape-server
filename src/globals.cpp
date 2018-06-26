#if defined (__cplusplus)
extern "C" {
#endif

  #include <stdio.h>

#if defined (__cplusplus)
}
#endif

#include "globals.h"

int interrupted;

void sigint_handler(int sig) {
  (void)sig;
  interrupted = 1;
}
