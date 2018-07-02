#include "device.h"

#include "file_ops.h"
#include "util.h"
#include "constant.h"

#include <fstream>
#include <algorithm>
#include <regex>

#include <iostream>
using std::cout;
using std::endl;

using std::pair;
using std::ofstream;
using std::regex;

recursive_mutex Device::mtx_;

Device::Device() {
  // As device_config is read only once we don't store or retain the duktape context
  duk_context_ = duk_create_heap_default();

  // Next few checks prevent some undefined behaviour thus throwing out of here
  // if they fail
  if (!duk_context_) {
    throw "Duk context could not be created.";
  }

  // loading configs
  map<string,map<string,string> > config = get_config(duk_context_);

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

  bool dexists = deviceExists();

  if(!dexists) {
    setDevId("");
    deleteDataField(Constant::Attributes::DEVICE_ID);

    sendDeviceInfo();
  }

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

thread* Device::getHttpClientThread() {
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  thread::id this_id = std::this_thread::get_id();

  if(getHttpClientThreads().find(this_id) != getHttpClientThreads().end()) {
    return getHttpClientThreads().at(this_id);
  }

  return 0;
}

void Device::spawnHttpClientThread() {
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  thread::id this_id = std::this_thread::get_id();

  if(getHttpClientThreads().find(this_id) != getHttpClientThreads().end() && !getHttpClientThreads().at(this_id)) {
    http_client_threads_.at(this_id) = new thread(&HttpClient::run, getHttpClient(), getCRConfig());
    return;
  } else if(getHttpClientThreads().find(this_id) != getHttpClientThreads().end()) {
    thread *clthread = getHttpClientThreads().at(this_id);
    if(clthread) {
      if(clthread->joinable()) {
        try {
          clthread->join();
          delete clthread;
        } catch(std::system_error e) {
          clthread->detach();
          clthread = 0;
        }
      }

      http_client_threads_.at(this_id) = 0;
    }

    http_client_threads_.at(this_id) = new thread(&HttpClient::run, getHttpClient(), getCRConfig());
    return;
  }

  http_client_threads_.insert(pair<thread::id,thread*>(this_id, new thread(&HttpClient::run, getHttpClient(), getCRConfig())));
  return;
}

bool Device::sendDeviceInfo() {
  // making sure we have nothing to worry about
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload(getDeviceInfoAsJSON());
  getCRConfig()->setRequestType("POST");
  getCRConfig()->setRequestPath("/devices");

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

  if(getHttpClientThread()->joinable())
    getHttpClientThread()->join();

  if(getCRConfig()->getResponseStatus()  == 200) {
    cleanDeviceId(getCRConfig()->getResponse());
    return true;
  }

  return false;
}

bool Device::deviceExists() {
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload("");
  getCRConfig()->setRequestType("GET");
  string srpath = (string("/devices/id/")+getDevId());
  const char * rpath = srpath.c_str();
  getCRConfig()->setRequestPath(rpath);

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

  if(getHttpClientThread()->joinable())
    getHttpClientThread()->join();

  // cleaning the response out of spaces
  string resp = getCRConfig()->getResponse();
  resp.erase(std::remove(resp.begin(),
    resp.end(), ' '), resp.end());

  return resp != "null" && getCRConfig()->getResponseStatus() == 200;
}

bool Device::appExists(string app_id) {
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload("");
  getCRConfig()->setRequestType("GET");
  string srpath = (string("/devices/") + getDevId() + string("/apps/") + app_id + string("/api"));
  const char * rpath = srpath.c_str();
  getCRConfig()->setRequestPath(rpath);

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

  if(getHttpClientThread()->joinable())
    getHttpClientThread()->join();

  return getCRConfig()->getResponseStatus() == 200;
}

bool Device::registerAppApi(string class_name, string swagger_fragment) {
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload(swagger_fragment);
  getCRConfig()->setRequestType("PUT");
  string srpath = (string("/apis/")+ class_name);
  const char * rpath = srpath.c_str();
  getCRConfig()->setRequestPath(rpath);

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

  if(getHttpClientThread()->joinable())
    getHttpClientThread()->join();

  return getCRConfig()->getResponseStatus() == 200;
}

bool Device::registerApp(string app_payload) {
  if(getDevId().length() == 0) return false;

  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());

  getCRConfig()->setRawPayload(app_payload);
  getCRConfig()->setRequestType("POST");
  string srpath = (string("/devices/")+ getDevId() + "/apps");
  const char * rpath = srpath.c_str();
  getCRConfig()->setRequestPath(rpath);

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

  if(getHttpClientThread()->joinable())
    getHttpClientThread()->join();

  return getCRConfig()->getResponseStatus() == 200;
}

bool Device::updateApp(string app_id, string app_payload) {
  exitClientThread();
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  getCRConfig()->setRawPayload(app_payload);
  getCRConfig()->setRequestType("PUT");
  char rpath[100];
  snprintf(rpath,100,Constant::Paths::UPDATE_APP_INFO_URL,Constant::Paths::DEV_ROOT_URL,getDevId().c_str(),stoi(app_id));
  getCRConfig()->setRequestPath(rpath);

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

  if(getHttpClientThread()->joinable())
    getHttpClientThread()->join();

  return getCRConfig()->getResponseStatus() == 200;
}

void Device::saveSettings() {
  map<string,map<string,string> > config = get_config(getContext());
  config.erase(Constant::Attributes::DEVICE);
  config.insert(pair<string,map<string,string> >(Constant::Attributes::DEVICE,getRawData()));
  save_config(getContext(),config);
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
  std::lock_guard<recursive_mutex> device_lock(getMutex());
  thread* http_ct = getHttpClientThread();
  if(http_ct) {
    if(http_ct->joinable())
      http_ct->join();
    delete http_ct;
    http_ct = 0;
  }
}
