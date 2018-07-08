/*
 *  Main starting point for liquidiot server
 */

#include <string>
#include <thread>

using std::string;
using std::thread;

#include "prints.h"
#include "applications_manager.h"
#include "device.h"
#include "http_server.h"

int main(int argc, char *argv[]) {

  (void) argc; (void) argv;  /* suppress warning */

  /* As there is only one device this is running at one time it makes sense to
   * make AppManager and Device singleton instances to reduce confusion and
   * improve accessability of some properties
   */


   // calling device constructor to get it ready
  try {
    Device::getInstance();
  } catch( char const * e) {
    ERROUT("Device error: " << e);
    return 1;
  }

  // creates and stores all the applications
  AppManager::getInstance();

  // Http server
  HttpServer *server = new HttpServer();
  thread *servert = new thread(&HttpServer::run, server);

  servert->join();
  JSApplication::getJoinThreads();

  // AppManager::getInstance().stopApps();

  return 0;
}
