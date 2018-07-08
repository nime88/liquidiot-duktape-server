/*
 *  Main starting point for liquidiot server
 */

#include <string>
#include <thread>

/*
 * Boost
 */
#include <boost/filesystem/path.hpp>
// #include <boost/dll/runtime_symbol_info.hpp>
namespace fs = boost::filesystem;
// namespace dll = boost::dll;

using std::string;
using std::thread;

#include "prints.h"
#include "applications_manager.h"
#include "device.h"
#include "http_server.h"

#include <unistd.h>

int main(int argc, char *argv[]) {
  // int PATH_MAX = 1024;
  char path[PATH_MAX];
  char dest[PATH_MAX];
  memset(dest,0,sizeof(dest)); // readlink does not null terminate!
  pid_t pid = getpid();
  sprintf(path, "/proc/%d/exe", pid);
  if (readlink(path, dest, PATH_MAX) == -1) {
    perror("readlink");
    return 1;
  }

  (void) argc; (void) argv;  /* suppress warning */

  /* As there is only one device this is running at one time it makes sense to
   * make AppManager and Device singleton instances to reduce confusion and
   * improve accessability of some properties
   */

  // fs::path full_path = dll::program_location();
  fs::path full_path = fs::path{dest};
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
  JSApplication::getJoinThreads();

  // AppManager::getInstance().stopApps();

  return 0;
}
