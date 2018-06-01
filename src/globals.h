#if !defined(GLOBALS_H)
#define GLOBALS_H

// some interruption global event handling
extern int interrupted;
extern void sigint_handler(int sig);

#endif
