#if !defined(APPLICATIONS_MANAGER_H_INCLUDED)
#define APPLICATIONS_MANAGER_H_INCLUDED

#include <vector>
#include <string>

#include "application.h"

using namespace std;

class AppManager {
  public:
    AppManager() {
      // loading applications
      loadApplications();
    }

    // lists and regenerates all the applications folder names
    vector<string> listApplicationNames();

    // loads the applications to usable "executables"
    void loadApplications();

    void addApp(JSApplication* app);

    vector<JSApplication*> getApps() {
      return apps_;
    }

  private:
    vector<string> app_names_;
    vector<JSApplication*> apps_;

};

#endif
