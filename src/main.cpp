/*
 *  Main starting point for liquidiot server
 */

#include <string>
#include <thread>

using std::string;
using std::thread;

#include "applications_manager.h"
#include "device.h"
#include "http_server.h"

int main(int argc, char *argv[]) {

  (void) argc; (void) argv;  /* suppress warning */

  /* As there is only one device this is running at one time it makes sense to
   * make AppManager and Device singleton instances to reduce confusion and
   * improve accessability of some properties
   */

  // creates and stores all the applications
  AppManager *app_manager = AppManager::getInstance();
  // Device device = Device::getInstance();
  Device::getInstance();

  // Http server
  HttpServer *server = new HttpServer();
  thread *servert = new thread(&HttpServer::run, server);

  JSApplication::getJoinThreads();
  servert->join();

  app_manager->stopApps();

  return 0;
}
