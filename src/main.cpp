/*
 *  Main starting point for liquidiot server
 */

#include <string>
#include <thread>

using std::string;
using std::thread;

#include "applications_manager.h"
#include "http_server.h"

int main(int argc, char *argv[]) {

  (void) argc; (void) argv;  /* suppress warning */

  AppManager *app_manager = new AppManager();

  // Http server
  HttpServer *server = new HttpServer();
  thread *servert = new thread(&HttpServer::run, server);

  JSApplication::getJoinThreads();
  servert->join();

  app_manager->stopApps();

  return 0;
}
