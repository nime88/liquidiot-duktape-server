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

Device *Device::instance_ = 0;
mutex *Device::mtx_ = new mutex();

Device::Device():id_("") {
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
    manager_server_config_ = config.at(Constant::Attributes::MANAGER_SERVER);

  if(config.find(Constant::Attributes::DEVICE) != config.end())
    raw_data_ = config.at(Constant::Attributes::DEVICE);

  // reading device data
  if(raw_data_.find(Constant::Attributes::DEVICE_ID) != raw_data_.end()) {
    setDeviceId(raw_data_.at(Constant::Attributes::DEVICE_ID));
  }

  if(raw_data_.find(Constant::Attributes::DEVICE_NAME) != raw_data_.end()) {
    name_ = raw_data_.at(Constant::Attributes::DEVICE_NAME);
  }

  if(raw_data_.find(Constant::Attributes::DEVICE_MANUFACTURER) != raw_data_.end()) {
    manufacturer_ = raw_data_.at(Constant::Attributes::DEVICE_MANUFACTURER);
  }

  if(raw_data_.find(Constant::Attributes::DEVICE_LOCATION) != raw_data_.end()) {
    location_ = raw_data_.at(Constant::Attributes::DEVICE_LOCATION);
  }

  if(raw_data_.find(Constant::Attributes::DEVICE_URL) != raw_data_.end()) {
    url_ = raw_data_.at(Constant::Attributes::DEVICE_URL);
  }

  if(raw_data_.find(Constant::Attributes::DEVICE_HOST) != raw_data_.end()) {
    host_ = raw_data_.at(Constant::Attributes::DEVICE_HOST);
  }

  if(raw_data_.find(Constant::Attributes::DEVICE_PORT) != raw_data_.end()) {
    port_ = raw_data_.at(Constant::Attributes::DEVICE_PORT);
  }

  bool dexists = deviceExists();

  if(!dexists) {
    if(raw_data_.find(Constant::Attributes::DEVICE_ID) != raw_data_.end()) {
      id_ = "";
      raw_data_.erase(Constant::Attributes::DEVICE_ID);
    }

    sendDeviceInfo();
  }

}

bool Device::sendDeviceInfo() {
  // making sure we have nothing to worry about
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,0,1); }
  // creating first connection to
  crconfig_ = new ClientRequestConfig();

  crconfig_->setRawPayload(getDeviceInfoAsJSON());
  crconfig_->setRequestType("POST");
  crconfig_->setRequestPath("/devices");

  if(manager_server_config_.find(Constant::Attributes::RR_HOST) != manager_server_config_.end()) {
    crconfig_->setRRHost(manager_server_config_.at(Constant::Attributes::RR_HOST).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(manager_server_config_.find(Constant::Attributes::RR_PORT) != manager_server_config_.end()) {
    crconfig_->setRRPort(manager_server_config_.at(Constant::Attributes::RR_PORT).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(!http_client_)
    http_client_ = new HttpClient();

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  if(crconfig_->getResponseStatus()  == 200) {
    setDeviceId(crconfig_->getResponse());
    return true;
  }

  return false;
}

bool Device::deviceExists() {
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,0,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload("");
  crconfig_->setRequestType("GET");
  string srpath = (string("/devices/id/")+id_);
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);

  if(manager_server_config_.find(Constant::Attributes::RR_HOST) != manager_server_config_.end()) {
    crconfig_->setRRHost(manager_server_config_.at(Constant::Attributes::RR_HOST).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(manager_server_config_.find(Constant::Attributes::RR_PORT) != manager_server_config_.end()) {
    crconfig_->setRRPort(manager_server_config_.at(Constant::Attributes::RR_PORT).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  // cleaning the response out of spaces
  string resp = crconfig_->getResponse();
  resp.erase(std::remove(resp.begin(),
    resp.end(), ' '), resp.end());

  return resp != "null" && crconfig_->getResponseStatus() == 200;
}

bool Device::appExists(string app_id) {
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,0,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload("");
  crconfig_->setRequestType("GET");
  string srpath = (string("/devices/") + id_ + string("/apps/") + app_id + string("/api"));
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);

  if(manager_server_config_.find(Constant::Attributes::RR_HOST) != manager_server_config_.end()) {
    crconfig_->setRRHost(manager_server_config_.at(Constant::Attributes::RR_HOST).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(manager_server_config_.find(Constant::Attributes::RR_PORT) != manager_server_config_.end()) {
    crconfig_->setRRPort(manager_server_config_.at(Constant::Attributes::RR_PORT).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  return crconfig_->getResponseStatus() == 200;
}

bool Device::registerAppApi(string class_name, string swagger_fragment) {
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,0,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload(swagger_fragment);
  crconfig_->setRequestType("PUT");
  string srpath = (string("/apis/")+ class_name);
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);

  if(manager_server_config_.find(Constant::Attributes::RR_HOST) != manager_server_config_.end()) {
    crconfig_->setRRHost(manager_server_config_.at(Constant::Attributes::RR_HOST).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(manager_server_config_.find(Constant::Attributes::RR_PORT) != manager_server_config_.end()) {
    crconfig_->setRRPort(manager_server_config_.at(Constant::Attributes::RR_PORT).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  return crconfig_->getResponseStatus() == 200;
}

bool Device::registerApp(string app_payload) {
  if(id_.length() == 0) return false;

  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,0,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload(app_payload);
  crconfig_->setRequestType("POST");
  string srpath = (string("/devices/")+ id_ + "/apps");
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);

  if(manager_server_config_.find(Constant::Attributes::RR_HOST) != manager_server_config_.end()) {
    crconfig_->setRRHost(manager_server_config_.at(Constant::Attributes::RR_HOST).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(manager_server_config_.find(Constant::Attributes::RR_PORT) != manager_server_config_.end()) {
    crconfig_->setRRPort(manager_server_config_.at(Constant::Attributes::RR_PORT).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  return crconfig_->getResponseStatus() == 200;
}

bool Device::updateApp(string app_id, string app_payload) {
  exitClientThread();

  while(getMutex()->try_lock()) { poll(NULL,0,1); }

  if(!crconfig_) {
    // creating first connection to
    crconfig_ = new ClientRequestConfig();
  }

  crconfig_->setRawPayload(app_payload);
  crconfig_->setRequestType("PUT");
  string srpath = (string("/devices/") + id_ + "/apps/" + app_id);
  const char * rpath = srpath.c_str();
  crconfig_->setRequestPath(rpath);

  if(manager_server_config_.find(Constant::Attributes::RR_HOST) != manager_server_config_.end()) {
    crconfig_->setRRHost(manager_server_config_.at(Constant::Attributes::RR_HOST).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(manager_server_config_.find(Constant::Attributes::RR_PORT) != manager_server_config_.end()) {
    crconfig_->setRRPort(manager_server_config_.at(Constant::Attributes::RR_PORT).c_str());
  } else {
    getMutex()->unlock();
    return false;
  }

  if(!http_client_) {
    http_client_ = new HttpClient();
  }

  http_client_thread_ = new thread(&HttpClient::run,http_client_,crconfig_);

  if(http_client_thread_->joinable())
    http_client_thread_->join();

  getMutex()->unlock();

  return crconfig_->getResponseStatus() == 200;
}

void Device::saveSettings() {
  map<string,map<string,string> > config = get_config(getContext());
  config.erase(Constant::Attributes::DEVICE);
  config.insert(pair<string,map<string,string> >(Constant::Attributes::DEVICE,raw_data_));
  save_config(getContext(),config);
}

string Device::getDeviceInfoAsJSON() {
  duk_context *ctx = getContext();

  duk_push_object(ctx);

  for ( map<string,string>::iterator it = raw_data_.begin(); it != raw_data_.end(); ++it) {
    duk_push_string(ctx,it->second.c_str());
    duk_put_prop_string(ctx, -2, it->first.c_str());
  }

  string full_info = duk_json_encode(ctx, -1);

  duk_pop(ctx);

  return full_info;
}

void Device::setDeviceId(string id) {
  // cleaning id
  std::smatch m;
  std::regex e( R"(^([A-Za-z0-9]+))" );

  string temp_id = id;
  while (std::regex_search (temp_id,m,e, std::regex_constants::match_any)) {
    id = m[1];
    temp_id = m.suffix().str();
  }

  while(getMutex()->try_lock()) { poll(NULL,0,1); }
  id_ = id;

  if(raw_data_.find(Constant::Attributes::DEVICE_ID) != raw_data_.end())
    raw_data_.at(Constant::Attributes::DEVICE_ID) = id_;
  else
    raw_data_.insert(pair<string,string>(Constant::Attributes::DEVICE_ID,id_));

  saveSettings();
  getMutex()->unlock();
}

void Device::exitClientThread() {
  while(getMutex()->try_lock()) { poll(NULL,0,1); }
  if(http_client_thread_) {
    if(http_client_thread_->joinable())
      http_client_thread_->join();
    delete http_client_thread_;
    http_client_thread_ = 0;
  }
  getMutex()->unlock();
}
