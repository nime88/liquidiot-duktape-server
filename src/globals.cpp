#if defined (__cplusplus)
extern "C" {
#endif

  #include <stdio.h>

#if defined (__cplusplus)
}
#endif

#include "globals.h"

int interrupted;
const sigset_t *sigmask = new sigset_t;

void sigint_handler(int sig) {
  (void)sig;
  interrupted = 1;
  sigprocmask(SIG_SETMASK, sigmask, NULL);
}
