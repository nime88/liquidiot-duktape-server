#if !defined(APPLICATIONS_MANAGER_H_INCLUDED)
#define APPLICATIONS_MANAGER_H_INCLUDED

#include <vector>
#include <string>

#include "application.h"

using std::vector;
using std::string;

class AppManager {
  public:
    AppManager() {
      // loading applications
      loadApplications();
    }

    // lists and regenerates all the applications folder names
    static vector<string> listApplicationNames();

    // loads the applications to usable "executables"
    void loadApplications();

    // TODO: In practice only one app works properly with intervals
    // as they are static atm
    void addApp(JSApplication* app);

    vector<JSApplication*> getApps() {
      return apps_;
    }

    void stopApps() {
      unsigned int num_apps = apps_.size();

      for(unsigned int i = 0; i < num_apps; ++i) {
        delete apps_.at(i);
        apps_.at(i) = 0;
      }
    }

  private:
    vector<string> app_names_;
    vector<JSApplication*> apps_;

};

#endif
