/*
 *  Main starting point for liquidiot server
 */

#include <string>
#include <thread>

/*
 * Boost
 */
#include <boost/filesystem/path.hpp>
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
namespace fs = boost::filesystem;
namespace dll = boost::dll;

using std::string;
using std::thread;

#include "prints.h"
#include "applications_manager.h"
#include "device.h"
#include "http_server.h"

#include <unistd.h>

int main(int argc, char *argv[]) {

  (void) argc; (void) argv;  /* suppress warning */

  /* As there is only one device this is running at one time it makes sense to
   * make AppManager and Device singleton instances to reduce confusion and
   * improve accessability of some properties
   */

  fs::path full_path = dll::program_location();

  full_path.remove_filename();
   // calling device constructor to get it ready
  try {
    Device::getInstance().setExecPath(full_path.string());
    Device::getInstance().init();
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
  delete servert;
  servert = 0;

  JSApplication::getJoinThreads();

  delete server;
  server = 0;

  return 0;
}
