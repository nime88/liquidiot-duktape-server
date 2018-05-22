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

#include "applications_manager.h"

int main(int argc, char *argv[]) {

  (void) argc; (void) argv;  /* suppress warning */

  // directly executing js file for testing purposes
  // duk_manager->executeFile("applications/test/main.js");

  AppManager *app_manager = new AppManager();

  unsigned int num_apps = app_manager->getApps().size();

  // executing all the applications source code
  for(unsigned int i = 0; i < num_apps; ++i) {
    app_manager->getApps().at(i)->run();
    // duk_manager->executeSource(app_manager->getApps().at(i)->getJSSource());
  }

  app_manager->stopApps();

  return 0;
}
