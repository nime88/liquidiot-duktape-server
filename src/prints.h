#ifndef PRINTS_H_INCLUDED
#define PRINTS_H_INCLUDED

#include <iostream>

// #define NDEBUG
#ifdef NDEBUG
    #define DBOUT( x ) std::cout << "\033[1;36m" << x  << "\033[0m\n"
#else
    #define DBOUT( x )
#endif

#define NERROR
#ifdef NERROR
  #define ERROUT( x ) std::cerr << "\033[1;31m" << x  << "\033[0m\n"
#else
    #define ERROUT( x )
#endif

#define NINFO
#ifdef NINFO
  #define INFOOUT( x ) std::cout << "\033[1;32m" << x  << "\033[0m\n"
#else
    #define INFOOUT( x )
#endif

#endif
