#if !defined(APPLICATIONS_MANAGER_H_INCLUDED)
#define APPLICATIONS_MANAGER_H_INCLUDED

#include <vector>
#include <string>

#include "application.h"

using std::vector;
using std::string;

class AppManager {
  public:

    // app manager is singleton instance to make some stuff more accessable
    // from different threads
    inline static AppManager& getInstance() {
      static AppManager instance;

      return instance;
    }

    AppManager(AppManager const&) = delete;
    void operator=(AppManager const&) = delete;

    // loads the applications to usable "executables"
    void loadApplications();

    const vector<JSApplication*>& getApps() {
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
    vector<JSApplication*> apps_;

    // constructor is private so it can't be instantiated outside of this class
    AppManager() {
      // loading applications
      loadApplications();
    }
};

#endif
