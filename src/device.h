#ifndef DEVICE_H_INCLUDED
#define DEVICE_H_INCLUDED

#include "duktape.h"

#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

using std::string;
using std::map;
using std::vector;
using std::thread;
using std::mutex;
using std::recursive_mutex;
using std::condition_variable;

#include "client_request_config.h"
#include "http_client.h"

class Device {
  public:
    inline static Device& getInstance() {
      static Device instance;
      return instance;
    }

    ~Device();

    Device(Device const&) = delete;
    void operator=(Device const&) = delete;

    void init();

    /* ******************* */
    /* Setters and Getters */
    /* ******************* */

    inline duk_context* getContext() { return duk_context_; }

    inline const string& getExecPath() { return exec_path_; }
    inline void setExecPath(const string& path) { exec_path_ = path; }

    inline const map<string,string>& getRawData() { return raw_data_; }
    inline void setRawData(const map<string,string>& data) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      raw_data_ = data;
    }
    void setRawDataField(const string& key, const string& value);
    void deleteDataField(const string& key);

    inline const map<string,string>& getManagerServerConfig() { return manager_server_config_; }
    inline void setManagerServerConfig(const map<string,string>& data) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      manager_server_config_ = data;
    }

    inline const string& getDevId() { return id_; }
    inline void setDevId(const string& id) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      id_ = id;
    }
    void cleanDeviceId(const string& id);

    inline const string& getDevName() { return name_; }
    inline void setDevName(const string& name) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      name_ = name;
    }

    inline const string& getDevManufacturer() { return manufacturer_; }
    inline void setDevManufacturer(const string& manufacturer) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      manufacturer_ = manufacturer;
    }

    inline const map<string,string>& getDevLocation() { return location_; }
    inline void setDevLocation(const map<string,string>& location) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      location_ = location;
    }

    inline const string& getDevUrl() { return url_; }
    inline void setDevUrl(const string& url) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      url_ = url;
    }

    inline const string& getDevHost() { return host_; }
    inline void setDevHost(const string& host) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      host_ = host;
    }

    inline const string& getDevPort() { return port_; }
    inline void setDevPort(const string& port) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      port_ = port;
    }

    inline const vector<string>& getDevCapabilities() { return classes_; }
    inline void addDevCapability(const string& capability) { classes_.push_back(capability); }

    ClientRequestConfig* getCRConfig();
    inline const map<thread::id, ClientRequestConfig*>& getCRConfigs() { return crconfigs_; }

    HttpClient* getHttpClient();
    inline const map<thread::id,HttpClient*>& getHttpClients() { return http_clients_; }

    thread& getHttpClientThread();
    inline map<thread::id,thread>& getHttpClientThreads() { return http_client_threads_; }
    void spawnHttpClientThread();

    /** Functions that use the client **/

    bool sendDeviceInfo();
    bool deviceExists();
    bool appExists(string app_id);
    bool registerAppApi(string class_name, string swagger_fragment);
    bool registerApp(string app_payload);
    bool updateApp(string app_id, string app_payload);
    bool deleteApp(string app_id);

    /** Functions that use the client end **/

    void saveSettings();

    string getDeviceInfoAsJSON();

    inline recursive_mutex& getMutex() { return mtx_; }
    static inline mutex& getCVMutex() { return cv_mtx_; }
    static inline condition_variable& getCV() { return condvar_; }

    static void notify();

  private:
    // duk_context
    duk_context *duk_context_;
    string exec_path_;

    recursive_mutex mtx_;
    static mutex cv_mtx_;
    static condition_variable condvar_;

    map<string,string> raw_data_;
    map<string,string> manager_server_config_;

    string id_;
    string name_;
    string manufacturer_;
    // libraries are a bit weird concept with current implementation, however
    // adding this for future support
    map<string,string> libraries_;
    // physical location
    map<string,string> location_;
    // url (most likely this device)
    string url_;
    string host_;
    string port_;
    // for future support
    map<string,string> connected_devices_;
    // hardware capabilities
    vector<string> classes_;

    // some client stuff
    map<thread::id, ClientRequestConfig*> crconfigs_;
    map<thread::id,HttpClient*> http_clients_;
    map<thread::id,thread> http_client_threads_;

    Device();
    void exitClientThread();
};

#endif
