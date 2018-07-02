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
using std::recursive_mutex;

#include "client_request_config.h"
#include "http_client.h"

class Device {
  public:
    inline static Device& getInstance() {
      static Device instance;
      return instance;
    }

    ~Device() {

      //dont need duktape anymore
      duk_destroy_heap(duk_context_);
      duk_context_ = 0;

      for(map<thread::id,HttpClient*>::iterator it = http_clients_.begin(); it != http_clients_.end(); ++it) {
        if(it->second) delete it->second;
        it->second = 0;
      }

      for(map<thread::id,thread*>::iterator it = http_client_threads_.begin(); it != http_client_threads_.end(); ++it) {
        if(it->second) {
          if(it->second->joinable())
            it->second->join();
          delete it->second;
        }
        it->second = 0;
      }

    }

    Device(Device const&) = delete;
    void operator=(Device const&) = delete;


    /* ******************* */
    /* Setters and Getters */
    /* ******************* */

    inline duk_context* getContext() { return duk_context_; }

    inline const map<string,string>& getRawData() { return raw_data_; }
    void setRawData(const map<string,string>& data) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      raw_data_ = data;
    }
    void setRawDataField(const string& key, const string& value);
    void deleteDataField(const string& key);

    inline const map<string,string>& getManagerServerConfig() { return manager_server_config_; }
    void setManagerServerConfig(const map<string,string>& data) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      manager_server_config_ = data;
    }

    inline const string& getDevId() { return id_; }
    void setDevId(const string& id) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      id_ = id;
    }
    void cleanDeviceId(const string& id);

    inline const string& getDevName() { return name_; }
    void setDevName(const string& name) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      name_ = name;
    }

    inline const string& getDevManufacturer() { return manufacturer_; }
    void setDevManufacturer(const string& manufacturer) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      manufacturer_ = manufacturer;
    }

    inline const string& getDevLocation() { return location_; }
    void setDevLocation(const string& location) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      location_ = location;
    }

    inline const string& getDevUrl() { return url_; }
    void setDevUrl(const string& url) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      url_ = url;
    }

    inline const string& getDevHost() { return host_; }
    void setDevHost(const string& host) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      host_ = host;
    }

    inline const string& getDevPort() { return port_; }
    void setDevPort(const string& port) {
      std::lock_guard<recursive_mutex> device_lock(getMutex());
      port_ = port;
    }

    ClientRequestConfig* getCRConfig();
    inline const map<thread::id, ClientRequestConfig*>& getCRConfigs() { return crconfigs_; }

    HttpClient* getHttpClient();
    inline const map<thread::id,HttpClient*>& getHttpClients() { return http_clients_; }

    thread* getHttpClientThread();
    inline const map<thread::id,thread*>& getHttpClientThreads() { return http_client_threads_; }
    void spawnHttpClientThread();

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

    static inline recursive_mutex& getMutex() { return mtx_; }

  private:
    // duk_context
    duk_context *duk_context_;

    static recursive_mutex mtx_;

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
    map<thread::id, ClientRequestConfig*> crconfigs_;
    map<thread::id,HttpClient*> http_clients_;
    map<thread::id,thread*> http_client_threads_;

    Device();
    void exitClientThread();
};

#endif
