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

using std::string;
using std::map;
using std::vector;
using std::thread;

#include "client_request_config.h"
#include "http_client.h"

class Device {
  public:
    static Device* getInstance();

    void sendDeviceInfo();

    string getDeviceInfoAsJSON();

    inline duk_context* getContext() { return duk_context_; }

  private:
    static Device *instance_;

    map<string,string> raw_data_;

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

    ~Device() {
      //dont need duktape anymore
      duk_destroy_heap(duk_context_);
      duk_context_ = 0;
    }
};

#endif