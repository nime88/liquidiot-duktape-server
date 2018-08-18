#if !defined(APPLICATION_H_INCLUDED)
#define APPLICATION_H_INCLUDED

#include "prints.h"
#include "custom_eventloop.h"
class AppRequest;
#include "app_request.h"
class AppResponse;
#include "app_response.h"
#include "constant.h"
#include "mod_duk_console.h"
#include "device.h"

#if defined (__cplusplus)
extern "C" {
#endif

#include "duk_print_alert.h"
#include "duk_module_node.h"

#if defined (__cplusplus)
}
#endif

#include "duktape.h"

#include <string>
#include <vector>
#include <array>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>

using std::string;
using std::vector;
using std::array;
using std::map;
using std::thread;
using std::mutex;
using std::recursive_mutex;
using std::condition_variable;


class JSApplication {
  public:
    JSApplication(const char* path);
    JSApplication(const char* path, int id);
    ~JSApplication();

    // simply initializes the app(doesn't take path)
    bool init();
    void instantiate();

    // cleans the app from unnecessary stuff
    // void clean();

    enum APP_STATES { INITIALIZING=0, CRASHED, RUNNING, PAUSED };
    static const array<string,4> APP_STATES_CHAR;

    /* ******************* */
    /* Setters and Getters */
    /* ******************* */

    inline duk_context* getContext() { return duk_context_; }

    inline thread& getEventLoopThread() { return ev_thread_; }
    bool createEventLoopThread();
    bool deleteEventLoopThread();

    inline EventLoop* getEventLoop() { return eventloop_; }
    EventLoop* createEventLoop();
    bool deleteEventLoop();

    inline char* getPackageJSON() { return package_json_; }
    inline void setPackageJSON( char* package_json ) { package_json_ = package_json; }

    inline const char* getJSSource() { return source_code_; }
    void setJSSource(const char * source, int source_len);

    inline const string& getAppPath() { return app_path_; }
    inline void setAppPath(const string& path) { app_path_ = path; }

    inline const string& getAppName() { return name_; }
    void setAppName(const string& name);

    inline const string& getAppVersion() { return version_; }
    void setAppVersion(const string& version);

    inline const string& getAppDescription() { return description_; }
    void setAppDesciption(const string& description);

    inline const string& getAppAuthor() { return author_; }
    void setAppAuthor(const string& author);

    inline const string& getAppLicence() { return licence_; }
    void setAppLicence(const string& licence);

    inline const string& getAppMain() { return main_; }
    void setAppMain(const string& main);

    inline int getAppId() { return id_; }
    void setAppId(const string& id);

    inline APP_STATES getAppState() { return app_state_; }
    bool setAppState(APP_STATES state);

    inline const vector<string>& getAppInterfaces() { return application_interfaces_; }
    inline void setAppInterfaces(const vector<string> &ais) { application_interfaces_ = ais; }

    inline void addAppAPI(const string& rest_api) { rest_api_paths_.push_back(rest_api); }
    inline const vector<string>& getAppAPIs() { return rest_api_paths_; }

    inline const map<string,string>& getRawPackageJSONData() { return raw_package_json_data_; }
    void setRawPackageJSONDataField(const string& key, const string& value);
    inline void setRawPackageJSONData(const map<string,string>& data) { raw_package_json_data_ = data; }

    AppResponse* getResponse(AppRequest *request);
    void setResponseObj(AppResponse *response);

    inline const string& getSwaggerFragment() { return swagger_fragment_; }
    inline void setSwaggerFragment(const string& fragment) { swagger_fragment_ = fragment; }

    inline bool isReady() { return is_ready_; }
    inline void setIsReady(bool ready) { is_ready_ = ready; }

    static inline const map<string,string>& getOptions() { return options_; }
    static inline void setOptions(const map<string,string>& options) { options_ = options; }

    static inline const map<duk_context*, JSApplication*>& getApplications() { return applications_; }
    static void addApplication(JSApplication *app);
    // effectively deletes the app
    static bool deleteApplication(JSApplication *app);
    static bool shutdownApplication(JSApplication *app);

    inline static recursive_mutex& getStaticMutex() { return static_mutex_; }
    inline recursive_mutex& getAppMutex() { return app_mutex_; }
    inline recursive_mutex& getDuktapeMutex() { return duktape_mutex_; }
    inline mutex& getCVMutex() { return cv_mutex_; }
    inline condition_variable& getEventLoopCV() { return evcv_; }

    static int getNextId(bool generate_new);

    /* ***** */
    /* Other */
    /* ***** */

    string getLogs();
    string getLogsAsJSON();

    bool pause();
    bool start();

    void run();

    void updateAppState(APP_STATES state) { updateAppState(state,false); }
    void updateAppState(APP_STATES state, bool update_client);

    // formats
    string getDescriptionAsJSON();
    string getAppAsJSON();

    bool hasAI(const string &ai) {
      for (const string &s : getAppAPIs()) {
        if (s == ai) return true;
      }

      return false;
    }

    static duk_ret_t cb_resolve_module(duk_context *ctx);
    static duk_ret_t cb_load_module(duk_context *ctx);
    static duk_ret_t cb_print(duk_context *ctx);
    static duk_ret_t cb_alert(duk_context *ctx);

    static duk_ret_t cb_resolve_app_response(duk_context *ctx);
    static duk_ret_t cb_load_swagger_fragment(duk_context *ctx);
    static duk_ret_t cb_register_app_api(duk_context *ctx);

    static bool applicationExists(const char* path);
    static const vector<string>& listApplicationNames();

    static JSApplication* getApplicationById(int id) {
      {
        std::lock_guard<recursive_mutex> static_lock(getStaticMutex());
        for (  map<duk_context*, JSApplication*>::const_iterator it=getApplications().begin(); it!=getApplications().end(); ++it) {
          if (it->second->getAppId() == id) {
            return it->second;
          }
        }
      }
      return 0;
    }


    static void getJoinThreads();
    static void notify();

    JSApplication(JSApplication const&) = delete;
    void operator=(JSApplication const&) = delete;

  private:
    thread ev_thread_;
    thread js_instance_;
    EventLoop *eventloop_;
    duk_context *duk_context_;
    char* package_json_;
    char* source_code_;
    string app_path_;

    // outside app references
    string name_;
    string version_;
    string description_;
    string author_;
    string licence_;
    string main_;
    int id_ = -1;
    APP_STATES app_state_;
    vector<string> application_interfaces_;

    vector<string> rest_api_paths_;

    map<string,string> raw_package_json_data_;

    // other
    AppResponse *app_response_;
    string swagger_fragment_;
    bool is_ready_;


    // static
    static int next_id_;
    static map<string,string> options_;
    static map<duk_context*, JSApplication*> applications_;
    static vector<string> app_names_;

    // mutexes
    static recursive_mutex static_mutex_;
    recursive_mutex app_mutex_;
    recursive_mutex duktape_mutex_;
    mutex cv_mutex_;
    condition_variable evcv_;

    duk_idx_t set_task_interval_idx_;

    void defaultConstruct(const char* path);

    // gets the info from package.json
    void parsePackageJS();
};

#endif
