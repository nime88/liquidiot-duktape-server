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
#include <stdexcept>

using namespace std;

#include "applications_manager.h"
#include "application.h"
#include "file_ops.h"
#include "http_server.h"

int main(int argc, char *argv[]) {

  (void) argc; (void) argv;  /* suppress warning */

  // directly executing js file for testing purposes
  // duk_manager->executeFile("applications/test/main.js");

  AppManager *app_manager = new AppManager();

  // Http server
  HttpServer *server = new HttpServer();
  thread *servert = new thread(&HttpServer::run, server);

  JSApplication::getJoinThreads();
  servert->join();

  app_manager->stopApps();

  // map<string,string> options = load_config("./config.cfg");

  // cout << options << endl;

  return 0;
}
