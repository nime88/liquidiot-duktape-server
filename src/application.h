#if !defined(APPLICATION_H_INCLUDED)
#define APPLICATION_H_INCLUDED

#include "file_ops.h"
#include "nodejs/node_module_search.h"
#include "eventloop/custom_eventloop.h"
class AppRequest;
#include "http/app_request.h"
class AppResponse;
#include "http/app_response.h"

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
    string getLogs();
    string getLogsAsJSON();

    AppResponse* getResponse(AppRequest *request);

    APP_STATES getAppState() { return app_state_; }
    bool setAppState(APP_STATES state);

    bool pause();
    bool start();

    void run();
    // reloads the app from files
    void reload();
    // effectively deletes the app
    bool deleteApp();

    // formats
    string getDescriptionAsJSON();

    bool hasAI(const string &ai) {
      for (string &s : application_interfaces_) {
        if (s == ai) return true;
      }

      return false;
    }

    void setResponseObj(AppResponse *response) {
      // locking thread as it's possible that this is called ny something else 
      while (!mtx->try_lock()){ poll(NULL,0,1); }
      app_response_ = response;
      mtx->unlock();
    }

    static duk_ret_t cb_resolve_module(duk_context *ctx);
    static duk_ret_t cb_load_module(duk_context *ctx);
    static duk_ret_t cb_print(duk_context *ctx);
    static duk_ret_t cb_alert(duk_context *ctx);

    static duk_ret_t cb_resolve_app_response(duk_context *ctx);

    static bool applicationExists(const char* path);
    static vector<string> listApplicationNames();
    static map<duk_context*, JSApplication*> getApplications() { return applications_; }
    static JSApplication* getApplicationById(int id) {
      while (!mtx->try_lock()){
        poll(NULL,0,1);
      }
      for (  map<duk_context*, JSApplication*>::const_iterator it=applications_.begin(); it!=applications_.end(); ++it) {
        if (it->second->getId() == id) {
          mtx->unlock();
          return it->second;
        }
      }
      mtx->unlock();
      return 0;
    }

    static mutex* getMutex() { return mtx; }
    static void getJoinThreads();



  private:
    thread *ev_thread_;
    EventLoop *eventloop_;
    duk_context *duk_context_;
    char* package_js_;
    char* source_code_;
    string app_path_;

    // outside app references
    string name_;
    string version_;
    string description_;
    string main_;
    int id_;
    APP_STATES app_state_;
    vector<string> application_interfaces_;

    // other
    AppResponse *app_response_ = 0;


    // static
    static int next_id_;
    static map<string,string> options_;
    static map<duk_context*, JSApplication*> applications_;
    static map<duk_context*, thread*> app_threads_;
    static mutex *mtx;


    duk_idx_t set_task_interval_idx_;
};

#endif
