#include "applications_manager.h"

#include <dirent.h>
#include <stdlib.h>
#include <string>

#include <iostream>

vector<string> AppManager::listApplicationNames() {
  vector<string> names;

  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir ("applications")) != NULL) {
    // clearing old application names
    names.clear();

    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      string temp_name = ent->d_name;
      if(temp_name != "." && temp_name != "..") {
        names.push_back(ent->d_name);
      }
    }
    closedir (dir);
  } else {
    /* could not open directory */
    // perror ("");
    return names;
  }

  // storing the results
  app_names_ = names;

  return names;

}

void AppManager::loadApplications() {
  // loading the list of application names
  listApplicationNames();

  unsigned int num_app = app_names_.size();
  for(unsigned int i = 0; i < num_app; ++i) {
    std::cout << "Loading app... " << app_names_.at(i) << std::endl;
    std::string path = "applications/" + app_names_.at(i);
    JSApplication* temp_app = new JSApplication(path.c_str());
    if(temp_app) {
      addApp(temp_app);
    }
  }

}

void AppManager::addApp(JSApplication* app) {
  if(app) {
    apps_.push_back(app);
  }
}
