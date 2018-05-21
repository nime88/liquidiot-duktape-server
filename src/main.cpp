/*
 *  Very simple example program
 */

#if defined (__cplusplus)
extern "C" {
#endif

  #include <stdio.h>
  #include "duktape.h"
  #include "duk_print_alert.h"

#if defined (__cplusplus)
}
#endif

#include <iostream>
#include <vector>
#include <string>

using namespace std;

#include "duk_manager.h"
#include "applications_manager.h"

int main(int argc, char *argv[]) {
  DukManager *duk_manager;

  try {
    duk_manager = new DukManager();
  } catch(...) {
    return 1;
  }

  (void) argc; (void) argv;  /* suppress warning */

  duk_manager->executeFile("applications/test/main.js");

  AppManager *app_manager = new AppManager();

  unsigned int num_apps = app_manager->getApps().size();

  for(unsigned int i = 0; i < num_apps; ++i) {
    duk_manager->executeSource(app_manager->getApps().at(i)->getJSSource());
  }

  return 0;
}
