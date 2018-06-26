#if !defined(GLOBALS_H)
#define GLOBALS_H

#if defined (__cplusplus)
extern "C" {
#endif

  // some interruption global event handling
  extern int interrupted;
  extern void sigint_handler(int sig);

#if defined (__cplusplus)
}
#endif


#endif
