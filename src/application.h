#if !defined(APPLICATION_H_INCLUDED)
#define APPLICATION_H_INCLUDED

#include "file_ops.h"
#include "nodejs/node_module_search.h"
#include "eventloop/custom_eventloop.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include <stdio.h>
  #include <dirent.h>
  #include <stdlib.h>
  #include <poll.h>
  #include "duktape.h"
  #include "duk_print_alert.h"
  #include "duk_console.h"
  #include "duk_module_node.h"

#if defined (__cplusplus)
}
#endif

#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <map>
#include <thread>
#include <mutex>
using namespace std;

class JSApplication {
  public:
    JSApplication(const char* path);

    ~JSApplication() {
      clean();
    }

    // simply initializes the app(doesn't take path)
    void init();

    // cleans the app from unnecessary stuff
    void clean();

    enum APP_STATES { INITIALIZING=0, CRASHED, RUNNING, PAUSED};
    vector<string> APP_STATES_CHAR = {"initializing","crashed","running","paused"};

    const char* getJSSource() {
      return source_code_;
    }

    duk_context* getContext() { return duk_context_;}

    int getId() { return id_; }
    string getAppName() { return name_; }
    string getAppPath() { return app_path_; }

    APP_STATES getAppState() { return app_state_; }

    void initializeApp();
    void pauseApp();

    void run();
    // reloads the app from files
    void reload();

    static duk_ret_t cb_resolve_module(duk_context *ctx);
    static duk_ret_t cb_load_module(duk_context *ctx);

    static bool applicationExists(const char* path);
    static vector<string> listApplicationNames();
    static map<duk_context*, JSApplication*> getApplications() { return applications_; }

    static mutex* getMutex() { return mtx; }
    static void getJoinThreads();

  private:
    APP_STATES app_state_;
    thread *ev_thread_;
    EventLoop *eventloop_;
    string name_;
    duk_context *duk_context_;
    char* package_js_;
    char* source_code_;
    string app_path_;


    int id_;
    static int next_id_;
    static map<string,string> options_;
    static map<duk_context*, JSApplication*> applications_;
    static map<duk_context*, thread*> app_threads_;
    static mutex *mtx;


    duk_idx_t set_task_interval_idx_;
};

#endif
