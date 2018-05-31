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
#include <map>
#include <thread>

using namespace std;

#include "applications_manager.h"
#include "file_ops.h"
#include "http_server.h"

int main(int argc, char *argv[]) {

  (void) argc; (void) argv;  /* suppress warning */

  // directly executing js file for testing purposes
  // duk_manager->executeFile("applications/test/main.js");

  AppManager *app_manager = new AppManager();

  unsigned int num_apps = app_manager->getApps().size();

  vector<thread*> threads;

  // executing all the applications source code
  for(unsigned int i = 0; i < num_apps; ++i) {
    thread *t = new thread(&JSApplication::run,app_manager->getApps().at(i));
    threads.push_back(t);
    // duk_manager->executeSource(app_manager->getApps().at(i)->getJSSource());
  }

  // Testing http server
  HttpServer *server = new HttpServer();
  thread *servert = new thread(&HttpServer::run, server);
  threads.push_back(servert);

  // joining the threads to main thread
  for(unsigned int i = 0; threads.size(); ++i) {
    threads.at(i)->join();
  }

  app_manager->stopApps();

  // map<string,string> options = load_config("./config.cfg");

  // cout << options << endl;

  return 0;
}
