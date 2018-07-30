#include "applications_manager.h"

#include "prints.h"
#include "constant.h"

void AppManager::loadApplications() {
  // loading the list of application names
  vector<string> app_names = JSApplication::listApplicationNames();

  unsigned int num_app = app_names.size();
  for(unsigned int i = 0; i < num_app; ++i) {
    INFOOUT("Loading app " << app_names.at(i));
    std::string path = string(Constant::Paths::APPLICATIONS_ROOT) + "/" + app_names.at(i);
    // simply creating an app, its internal structure will handle rest
    JSApplication *app = 0;
    try {
      app = new JSApplication(path.c_str());
    } catch (const char * e) {
      if(app) delete app;
      ERROUT(e);
      continue;
    }

    apps_.push_back(app);
  }

}
