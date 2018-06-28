#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED

#if defined (__cplusplus)
extern "C" {
#endif

  #include "duktape.h"

#if defined (__cplusplus)
}
#endif

#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

using std::string;
using std::map;
using std::vector;
using std::thread;
using std::mutex;

#include "client_request_config.h"
#include "http_client.h"

class Device {
  public:
    static Device& getInstance() {
      static Device instance;

      return instance;
    }

    ~Device() {

      //dont need duktape anymore
      duk_destroy_heap(duk_context_);
      duk_context_ = 0;

      exitClientThread();

      if( http_client_ )
        delete http_client_;
      http_client_ = 0;

      if( mtx_ )
        delete mtx_;
      mtx_ = 0;
    }

    Device(Device const&) = delete;
    void operator=(Device const&) = delete;

    /** Functions that use the client **/

    bool sendDeviceInfo();
    bool deviceExists();
    bool appExists(string app_id);
    bool registerAppApi(string class_name, string swagger_fragment);
    bool registerApp(string app_payload);
    bool updateApp(string app_id, string app_payload);

    /** Functions that use the client end **/

    void saveSettings();

    string getDeviceInfoAsJSON();

    inline duk_context* getContext() { return duk_context_; }

    void setDeviceId(string id);

    static mutex* getMutex() { return mtx_; }

  private:
    static Device *instance_;
    static mutex *mtx_;

    map<string,string> raw_data_;
    map<string,string> manager_server_config_;

    string id_;
    string name_;
    string manufacturer_;
    // libraries are a bit weird concept with current implementation, however
    // adding this for future support
    map<string,string> libraries_;
    // physical location
    string location_;
    // url (most likely this device)
    string url_;
    string host_;
    string port_;
    // for future support
    map<string,string> connected_devices_;
    // hardware capabilities
    vector<string> classes_;

    // some client stuff
    ClientRequestConfig *crconfig_;
    HttpClient *http_client_ = 0;
    thread *http_client_thread_;

    // duk_context
    duk_context *duk_context_;

    Device();
    void exitClientThread();
};

#endif
