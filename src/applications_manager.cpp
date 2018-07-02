#include "applications_manager.h"

#include <dirent.h>
#include <iostream>

using std::cout;
using std::endl;

#include "constant.h"

void AppManager::loadApplications() {
  // loading the list of application names
  vector<string> app_names = JSApplication::listApplicationNames();

  unsigned int num_app = app_names.size();
  for(unsigned int i = 0; i < num_app; ++i) {
    std::cout << "Loading app... " << app_names.at(i) << std::endl;
    std::string path = string(Constant::Paths::APPLICATIONS_ROOT) + "/" + app_names.at(i);
    // simply creating an app, its internal structure will handle rest
    apps_.push_back(new JSApplication(path.c_str()));
  }

}
