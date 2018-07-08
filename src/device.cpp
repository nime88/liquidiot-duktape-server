#include "device.h"

#include "prints.h"
#include "file_ops.h"
#include "util.h"
#include "constant.h"
#include "globals.h"

#include <fstream>
#include <algorithm>
#include <regex>

using std::pair;
using std::ofstream;
using std::regex;

mutex Device::cv_mtx_;
condition_variable Device::condvar_;

Device::Device() {}

void Device::init() {
  // As device_config is read only once we don't store or retain the duktape context
  duk_context_ = duk_create_heap_default();

  // Next few checks prevent some undefined behaviour thus throwing out of here
  // if they fail
  if (!duk_context_) {
    ERROUT ("Device(): duk_context creation failed");
    throw "Duk context could not be created.";
  }

  DBOUT ("Device(): loading config");
  // loading configs
  map<string,map<string,string> > config;
  try {
    config = get_config(duk_context_, getExecPath());
  } catch (char const * e) {
    ERROUT("Device failed to read config: " << e);
    throw "Device failed to start.";
  }

  if(config.find(Constant::Attributes::MANAGER_SERVER) != config.end())
    setManagerServerConfig(config.at(Constant::Attributes::MANAGER_SERVER));

  if(config.find(Constant::Attributes::DEVICE) != config.end())
    setRawData(config.at(Constant::Attributes::DEVICE));

  // reading device data
  if(getRawData().find(Constant::Attributes::DEVICE_ID) != getRawData().end()) {
    setDevId(getRawData().at(Constant::Attributes::DEVICE_ID));
  }

  if(getRawData().find(Constant::Attributes::DEVICE_NAME) != getRawData().end()) {
    setDevName(getRawData().at(Constant::Attributes::DEVICE_NAME));
  }

  if(getRawData().find(Constant::Attributes::DEVICE_MANUFACTURER) != getRawData().end()) {
    setDevManufacturer(getRawData().at(Constant::Attributes::DEVICE_MANUFACTURER));
  }

  if(getRawData().find(Constant::Attributes::DEVICE_LOCATION) != getRawData().end()) {
    setDevLocation(getRawData().at(Constant::Attributes::DEVICE_LOCATION));
  }

  if(getRawData().find(Constant::Attributes::DEVICE_URL) != getRawData().end()) {
    setDevUrl(getRawData().at(Constant::Attributes::DEVICE_URL));
  }

  if(getRawData().find(Constant::Attributes::DEVICE_HOST) != getRawData().end()) {
    setDevHost(getRawData().at(Constant::Attributes::DEVICE_HOST));
  }

  if(getRawData().find(Constant::Attributes::DEVICE_PORT) != getRawData().end()) {
    setDevPort(getRawData().at(Constant::Attributes::DEVICE_PORT));
  }

  DBOUT ("Device(): checking if device already exists");
  bool dexists = deviceExists();

  if(!dexists) {
    DBOUT ("Device(): creating new device");
    setDevId("");
    deleteDataField(Constant::Attributes::DEVICE_ID);

    sendDeviceInfo();
  }

  DBOUT ("Device(): OK");
}

void Device::setRawDataField(const string& key, const string& value) {
  if(getRawData().find(key) != getRawData().end()) {
    std::lock_guard<recursive_mutex> device_lock(getMutex());
    raw_data_.at(key) = value;
    return;
  }

  std::lock_guard<recursive_mutex> device_lock(getMutex());
  raw_data_.insert(pair<string,string>(key,value));
}

void Device::deleteDataField(const string& key) {
  if(getRawData().find(key) != getRawData().end()) {
    std::lock_guard<recursive_mutex> device_lock(getMutex());
    raw_data_.erase(key);
  }
}

ClientRequestConfig* Device::getCRConfig() {
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  thread::id this_id = std::this_thread::get_id();

  if(getCRConfigs().find(this_id) != getCRConfigs().end()) {
    return getCRConfigs().at(this_id);
  } else crconfigs_.insert(pair<thread::id,ClientRequestConfig*>(this_id,new ClientRequestConfig()));

  return getCRConfigs().at(this_id);
}

HttpClient* Device::getHttpClient() {
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  thread::id this_id = std::this_thread::get_id();

  if(getHttpClients().find(this_id) != getHttpClients().end()) {
    return getHttpClients().at(this_id);
  } else http_clients_.insert(pair<thread::id,HttpClient*>(this_id,new HttpClient()));

  if(getHttpClients().find(this_id) != getHttpClients().end()) {
    return getHttpClients().at(this_id);
  }

  return 0;
}

thread& Device::getHttpClientThread() {
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  thread::id this_id = std::this_thread::get_id();

  if(getHttpClientThreads().find(this_id) != getHttpClientThreads().end()) {
    return getHttpClientThreads().at(this_id);
  }

  throw "No HttpClientThread found";
}

void Device::spawnHttpClientThread() {
  DBOUT("spawnHttpClientThread()");
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  thread::id this_id = std::this_thread::get_id();

  if(getHttpClientThreads().find(this_id) != getHttpClientThreads().end() && !getHttpClientThread().joinable()) {
    DBOUT("spawnHttpClientThread(): perfect setup");
    http_client_threads_.at(this_id) = thread(&HttpClient::run, getHttpClient(), getCRConfig());
    DBOUT("spawnHttpClientThread(): OK");
    return;
  } else if(getHttpClientThreads().find(this_id) != getHttpClientThreads().end()) {
    DBOUT("spawnHttpClientThread(): joining old thread");

    try {
      {
        HttpClient *client = getHttpClient();
        std::unique_lock<mutex> cv_lock(getCVMutex());
        getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
          DBOUT("spawnHttpClientThread(): is ready: " << client->isReady() );
          return !client || client->isReady()||interrupted;
        });
      }
      getCVMutex().unlock();
      if(getHttpClientThread().joinable()) {
          //BRB
          // clthread->detach();
          getHttpClientThread().join();
      }
    } catch (const std::system_error& e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("spawnHttpClientThread(): Caught a system_error");
        ERROUT("spawnHttpClientThread(): the error description is " << e.what());
      }
    }

    DBOUT("spawnHttpClientThread(): setting new thread on top of old one");
    http_client_threads_.at(this_id) = thread(&HttpClient::run, getHttpClient(), getCRConfig());
    DBOUT("spawnHttpClientThread(): OK");
    return;

  }

  DBOUT("spawnHttpClientThread(): creating completely new thread");
  http_client_threads_.insert(pair<thread::id,thread>(this_id, thread(&HttpClient::run, getHttpClient(), getCRConfig())));
  DBOUT("spawnHttpClientThread(): OK");
  return;
}

bool Device::sendDeviceInfo() {
  // making sure we have nothing to worry about
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload(getDeviceInfoAsJSON());
  getCRConfig()->setRequestType("POST");

  char rpath[100];
  snprintf(rpath,100,"/%s",Constant::Paths::DEV_ROOT_URL);
  getCRConfig()->setRequestPath(rpath);
  DBOUT( "sendDeviceInfo(): request path:" << rpath );

  if(getManagerServerConfig().find(Constant::Attributes::RR_HOST) != getManagerServerConfig().end()) {
    getCRConfig()->setRRHost(getManagerServerConfig().at(Constant::Attributes::RR_HOST).c_str());
  } else {
    return false;
  }

  if(getManagerServerConfig().find(Constant::Attributes::RR_PORT) != getManagerServerConfig().end()) {
    getCRConfig()->setRRPort(getManagerServerConfig().at(Constant::Attributes::RR_PORT).c_str());
  } else {
    return false;
  }

  spawnHttpClientThread();

  {
    HttpClient *client = getHttpClient();
    std::unique_lock<mutex> cv_lock(getCVMutex());
    getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
      return client->isReady()||interrupted;
    });
  }

  if(getHttpClientThread().joinable()) {
    try {
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("sendDeviceInfo(): Caught a system_error");
        ERROUT("sendDeviceInfo(): the error description is " << e.what());
        abort();
      }
    }
  }

  if(getCRConfig()->getResponseStatus()  == 200) {
    cleanDeviceId(getCRConfig()->getResponse());
    return true;
  }

  return false;
}

bool Device::deviceExists() {
  DBOUT( "deviceExists():");
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload("");
  getCRConfig()->setRequestType("GET");

  char rpath[100];
  snprintf(rpath,100,Constant::Paths::DEV_EXISTS_URL, Constant::Paths::DEV_ROOT_URL, getDevId().c_str());
  getCRConfig()->setRequestPath(rpath);
  DBOUT( "deviceExists(): request path:" << rpath );

  if(getManagerServerConfig().find(Constant::Attributes::RR_HOST) != getManagerServerConfig().end()) {
    getCRConfig()->setRRHost(getManagerServerConfig().at(Constant::Attributes::RR_HOST).c_str());
  } else {
    return false;
  }

  if(getManagerServerConfig().find(Constant::Attributes::RR_PORT) != getManagerServerConfig().end()) {
    getCRConfig()->setRRPort(getManagerServerConfig().at(Constant::Attributes::RR_PORT).c_str());
  } else {
    return false;
  }

  DBOUT( "deviceExists(): spawn new client thread:" << rpath );
  spawnHttpClientThread();

  {
    HttpClient *client = getHttpClient();
    std::unique_lock<mutex> cv_lock(getCVMutex());
    getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
      return client->isReady()||interrupted;
    });
  }
  DBOUT( "deviceExists(): unlocking:");
  getCVMutex().unlock();

  if(getHttpClientThread().joinable()) {
    try {
      DBOUT( "deviceExists(): trying to join client thread:" << rpath );
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("deviceExists(): Caught a system_error");
        ERROUT("deviceExists(): the error description is " << e.what());
        abort();
      }
    }
  }

  DBOUT( "deviceExists(): OK" << rpath );

  // cleaning the response out of spaces because for some reason response value
  // from the RR manager has some extra spaces
  string resp = getCRConfig()->getResponse();
  resp.erase(std::remove(resp.begin(),
    resp.end(), ' '), resp.end());

  return resp != "null" && getCRConfig()->getResponseStatus() == 200;
}

bool Device::appExists(string app_id) {
  DBOUT( "appExists(): app id:" << app_id );
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload("");
  getCRConfig()->setRequestType("GET");

  char rpath[100];
  snprintf(rpath,100,Constant::Paths::APP_EXISTS_URL, Constant::Paths::DEV_ROOT_URL, getDevId().c_str(), app_id.c_str());
  getCRConfig()->setRequestPath(rpath);
  DBOUT( "appExists(): request path:" << rpath );

  if(getManagerServerConfig().find(Constant::Attributes::RR_HOST) != getManagerServerConfig().end()) {
    getCRConfig()->setRRHost(getManagerServerConfig().at(Constant::Attributes::RR_HOST).c_str());
  } else {
    return false;
  }

  if(getManagerServerConfig().find(Constant::Attributes::RR_PORT) != getManagerServerConfig().end()) {
    getCRConfig()->setRRPort(getManagerServerConfig().at(Constant::Attributes::RR_PORT).c_str());
  } else {
    return false;
  }

  DBOUT( "appExists(): spawnHttpClientThread():");
  spawnHttpClientThread();

  {
    HttpClient *client = getHttpClient();
    std::unique_lock<mutex> cv_lock(getCVMutex());
    getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
      return client->isReady()||interrupted;
    });
  }
  getCVMutex().unlock();

  DBOUT( "appExists(): join():");
  if(getHttpClientThread().joinable()) {
    try {
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("appExists(): Caught a system_error");
        ERROUT("appExists(): the error description is " << e.what());
        abort();
      }
    }
  }

  DBOUT( "appExists(): OK:");
  return getCRConfig()->getResponseStatus() == 200;
}

bool Device::registerAppApi(string class_name, string swagger_fragment) {
  DBOUT( "registerAppApi(): class_name:" << class_name );
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload(swagger_fragment);
  getCRConfig()->setRequestType("PUT");

  char rpath[100];
  snprintf(rpath,100,Constant::Paths::REGISTER_APP_API_URL, class_name.c_str());
  getCRConfig()->setRequestPath(rpath);
  DBOUT( "registerAppApi(): request path:" << rpath );

  if(getManagerServerConfig().find(Constant::Attributes::RR_HOST) != getManagerServerConfig().end()) {
    getCRConfig()->setRRHost(getManagerServerConfig().at(Constant::Attributes::RR_HOST).c_str());
  } else {
    return false;
  }

  if(getManagerServerConfig().find(Constant::Attributes::RR_PORT) != getManagerServerConfig().end()) {
    getCRConfig()->setRRPort(getManagerServerConfig().at(Constant::Attributes::RR_PORT).c_str());
  } else {
    return false;
  }

  spawnHttpClientThread();

  {
    HttpClient *client = getHttpClient();
    std::unique_lock<mutex> cv_lock(getCVMutex());
    getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
      return client->isReady()||interrupted;
    });
  }
  getCVMutex().unlock();

  if(getHttpClientThread().joinable()) {
    try {
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("registerAppApi(): Caught a system_error");
        ERROUT("registerAppApi(): the error description is " << e.what());
        abort();
      }
    }
  }

  return getCRConfig()->getResponseStatus() == 200;
}

bool Device::registerApp(string app_payload) {
  if(getDevId().length() == 0) return false;

  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload(app_payload);
  getCRConfig()->setRequestType("POST");

  char rpath[100];
  snprintf(rpath,100,Constant::Paths::REGISTER_APP_URL,Constant::Paths::DEV_ROOT_URL,getDevId().c_str());
  getCRConfig()->setRequestPath(rpath);
  DBOUT( "registerApp(): request path:" << rpath );

  if(getManagerServerConfig().find(Constant::Attributes::RR_HOST) != getManagerServerConfig().end()) {
    getCRConfig()->setRRHost(getManagerServerConfig().at(Constant::Attributes::RR_HOST).c_str());
  } else {
    return false;
  }

  if(getManagerServerConfig().find(Constant::Attributes::RR_PORT) != getManagerServerConfig().end()) {
    getCRConfig()->setRRPort(getManagerServerConfig().at(Constant::Attributes::RR_PORT).c_str());
  } else {
    return false;
  }

  spawnHttpClientThread();

  {
    HttpClient *client = getHttpClient();
    std::unique_lock<mutex> cv_lock(getCVMutex());
    getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
      return client->isReady()||interrupted;
    });
  }
  getCVMutex().unlock();

  if(getHttpClientThread().joinable()) {
    try {
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("registerApp(): Caught a system_error");
        ERROUT("registerApp(): the error description is " << e.what());
        abort();
      }
    }
  }

  return getCRConfig()->getResponseStatus() == 200;
}

bool Device::updateApp(string app_id, string app_payload) {
  DBOUT( "updateApp(): app id:" << app_id );
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  getCRConfig()->setRawPayload(app_payload);
  getCRConfig()->setRequestType("PUT");
  char rpath[100];
  snprintf(rpath,100,Constant::Paths::UPDATE_APP_INFO_URL,Constant::Paths::DEV_ROOT_URL,getDevId().c_str(),stoi(app_id));
  getCRConfig()->setRequestPath(rpath);
  DBOUT( "updateApp(): request path:" << rpath );

  if(getManagerServerConfig().find(Constant::Attributes::RR_HOST) != getManagerServerConfig().end()) {
    getCRConfig()->setRRHost(getManagerServerConfig().at(Constant::Attributes::RR_HOST).c_str());
  } else {
    return false;
  }

  if(getManagerServerConfig().find(Constant::Attributes::RR_PORT) != getManagerServerConfig().end()) {
    getCRConfig()->setRRPort(getManagerServerConfig().at(Constant::Attributes::RR_PORT).c_str());
  } else {
    return false;
  }

  DBOUT( "updateApp(): spawn http client thread" );
  spawnHttpClientThread();

  {
    HttpClient *client = getHttpClient();
    std::unique_lock<mutex> cv_lock(getCVMutex());
    getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
      return client->isReady()||interrupted;
    });
  }
  getCVMutex().unlock();

  DBOUT( "updateApp(): trying to join client thread" );
  if(getHttpClientThread().joinable()) {
    try {
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("updateApp(): Caught a system_error");
        ERROUT("updateApp(): the error description is " << e.what());
        abort();
      }
    }
  }

  DBOUT( "updateApp(): succeed");
  return getCRConfig()->getResponseStatus() == 200;
}

bool Device::deleteApp(string app_id) {
  DBOUT( "deleteApp(): app id:" << app_id );
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload("");
  getCRConfig()->setRequestType("DELETE");
  char rpath[100];
  snprintf(rpath,100,Constant::Paths::DELETE_APP_URL,Constant::Paths::DEV_ROOT_URL,getDevId().c_str(),stoi(app_id));
  getCRConfig()->setRequestPath(rpath);
  DBOUT( "deleteApp(): request path:" << rpath );

  if(getManagerServerConfig().find(Constant::Attributes::RR_HOST) != getManagerServerConfig().end()) {
    getCRConfig()->setRRHost(getManagerServerConfig().at(Constant::Attributes::RR_HOST).c_str());
  } else {
    return false;
  }

  if(getManagerServerConfig().find(Constant::Attributes::RR_PORT) != getManagerServerConfig().end()) {
    getCRConfig()->setRRPort(getManagerServerConfig().at(Constant::Attributes::RR_PORT).c_str());
  } else {
    return false;
  }

  spawnHttpClientThread();

  {
    HttpClient *client = getHttpClient();
    std::unique_lock<mutex> cv_lock(getCVMutex());
    getCV().wait_for(cv_lock, std::chrono::seconds(1),[client]{
      return client->isReady()||interrupted;
    });
  }
  getCVMutex().unlock();

  if(getHttpClientThread().joinable()) {
    try {
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("deleteApp(): Caught a system_error");
        ERROUT("deleteApp(): the error description is " << e.what());
        abort();
      }
    }
  }

  return getCRConfig()->getResponseStatus() == 200;
}

void Device::saveSettings() {
  map<string,map<string,string> > config;
  try {
    config = get_config(getContext(), getExecPath());
  } catch (char const * e) {
    ERROUT("saveSettings(): Device failed to read config: " << e);
    return;
  }
  config.erase(Constant::Attributes::DEVICE);
  config.insert(pair<string,map<string,string> >(Constant::Attributes::DEVICE,getRawData()));
  try {
    save_config(getContext(),config, getExecPath());
  } catch (char const * e) {
    ERROUT("saveSettings(): Device failed to save config: " << e);
    return;
  }
}

string Device::getDeviceInfoAsJSON() {
  duk_context *ctx = getContext();

  duk_push_object(ctx);

  for ( map<string,string>::const_iterator it = getRawData().begin(); it != getRawData().end(); ++it) {
    duk_push_string(ctx,it->second.c_str());
    duk_put_prop_string(ctx, -2, it->first.c_str());
  }

  string full_info = duk_json_encode(ctx, -1);

  duk_pop(ctx);

  return full_info;
}

void Device::cleanDeviceId(const string& id) {
  // cleaning id
  std::smatch m;
  std::regex e( R"(^([A-Za-z0-9]+))" );

  string temp_id = id;
  string res_id;
  while (std::regex_search (temp_id,m,e, std::regex_constants::match_any)) {
    res_id = m[1];
    temp_id = m.suffix().str();
  }

  std::lock_guard<recursive_mutex> device_lock(getMutex());

  setDevId(res_id);
  setRawDataField(Constant::Attributes::DEVICE_ID, getDevId());
  saveSettings();
}

void Device::exitClientThread() {
  DBOUT("exitClientThread()");
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  try {
    getHttpClientThread();
  } catch( char const * e ) {
    DBOUT("exitClientThread(): no client thread: " << e);
    return;
  }

  if(getHttpClientThread().joinable()) {
    try {
      getHttpClientThread().join();
    } catch(std::system_error e) {
      if(std::errc::invalid_argument != e.code()) {
        ERROUT("exitClientThread(): Caught a system_error");
        ERROUT("exitClientThread(): the error description is " << e.what());
        abort();
      }
    }
  }
}

void Device::notify() {
  Device::getInstance().getCV().notify_all();
}
